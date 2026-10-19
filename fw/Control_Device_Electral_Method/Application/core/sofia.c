#include "sofia.h"
#include "sofia_io.h"
#include "board.h"
#include "power.h"
#include "vf_adc.h"
#include "vf_math.h"
#include "runtime_config.h"
#include "sdlog.h"
#include "format.h"
#include "hw_debug.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define IH_A 0.758f
#define IM_A 0.002f
static const uint32_t cooling_us[]={1000,2000,5000,10000,20000,50000,100000,200000,500000,1000000,2000000,5000000};
typedef struct {float vf,tc,tj;uint32_t from_cut_us;} point_t;
static point_t points[14];
static sofia_config_t cfg;
static job_status_t status;
static uint32_t start_ms,heat_ms,cut_ms,cut_cycles;
static unsigned count,cooling_index;
static uint32_t case_age_ms,reference_age_ms;
static bool open,measure;
volatile sofia_debug_t g_sofia_debug;
static void stop(void){power_safe();vf_adc_cancel_samples();if(open){(void)sdlog_close();open=false;}}
static job_status_t fail(unsigned e){g_sofia_debug.error=e;status=JOB_STATUS_FAILED;stop();return status;}
void sofia_init(void){status=JOB_STATUS_IDLE;open=measure=false;count=cooling_index=0;g_sofia_debug=(sofia_debug_t){0};}
static bool meta(const char *key,float value,unsigned decimals){char n[32],s[120];if(!format_decimal(n,sizeof(n),value,decimals))return false;(void)snprintf(s,sizeof(s),"#%s=%s",key,n);return sdlog_line(s);}
bool sofia_start(const sofia_config_t *c,const device_t *d){
 if(status==JOB_STATUS_RUNNING)return false;
 sofia_init();
 if(!c || !d || !isfinite(c->slope) || c->slope>=-1 || c->slope< -100000 ||
    !isfinite(c->intercept) || fabsf(c->intercept)>10000 || c->heat_s<1 || c->heat_s>600 ||
    c->delay_us<5 || c->delay_us>1000 || !isfinite(c->max_case_c) || c->max_case_c<30 || c->max_case_c>120 ||
    !isfinite(d->temperature_c) || d->temperature_c>=c->max_case_c){(void)fail(JOB_ERROR_INVALID_PARAMS);return false;}
 cfg=*c;start_ms=board_millis();
 if(!sdlog_ready() || !sdlog_open_run(false)){(void)fail(JOB_ERROR_STORAGE);return false;}open=true;
 if(!sdlog_line("#mode=TSP_STEP") || !sdlog_line("#case_sensor=MAX31855_CASE") ||
    !sdlog_line("#current_source=FIXED_DEVICE_CURRENTS") || !sdlog_line("#calibration_source=SOFIA_SETUP_UI") || !sdlog_line("#steady_state_verified=0") ||
    !meta("current_heating_a",IH_A,6) || !meta("current_measurement_a",IM_A,6) ||
    !meta("slope_c_per_v",cfg.slope,6) || !meta("intercept_c",cfg.intercept,6) ||
    !meta("delay_requested_us",(float)cfg.delay_us,0) || !meta("heat_requested_s",(float)cfg.heat_s,0)){
     (void)fail(JOB_ERROR_STORAGE);return false;}
 if(!vf_adc_ready()){(void)fail(JOB_ERROR_SAMPLE_FAILED);return false;}
 if(!power_enable_measurement() || !power_relay_on()){(void)fail(JOB_ERROR_POWER_LOCKED);return false;}
 /* SD creation and relay settling may take longer than the VREF age limit. */
 if(!vf_adc_refresh_reference()){(void)fail(JOB_ERROR_SAMPLE_FAILED);return false;}
 board_delay_us(30);float vf;
 if(!vf_adc_capture(8,&vf) || !isfinite(vf) || vf<=0){(void)fail(JOB_ERROR_SAMPLE_FAILED);return false;}
 float baseline=cfg.slope*vf+cfg.intercept;
 if(!isfinite(baseline) || baseline<TEMP_MIN_C || baseline>=OVERTEMP_C){(void)fail(JOB_ERROR_OVERTEMP);return false;}
 points[0]=(point_t){vf,d->temperature_c,baseline,0};count=1;
 if(!power_select_heating()){(void)fail(JOB_ERROR_POWER_LOCKED);return false;}
 heat_ms=board_millis();status=JOB_STATUS_RUNNING;g_sofia_debug.phase=1;return true;
}
void sofia_measure_now(void){if(status==JOB_STATUS_RUNNING && g_sofia_debug.phase==1)measure=true;}
void sofia_abort(void){if(status==JOB_STATUS_RUNNING){status=JOB_STATUS_ABORTED;stop();}}
static bool flush(void){
 if(!meta("heating_voltage_v",g_sofia_debug.heating_v,6) || !meta("heating_power_w",g_sofia_debug.heating_w,6) ||
    !meta("heat_actual_s",(float)(cut_ms-heat_ms)/1000,6) || !meta("delay_first_us",(float)g_sofia_debug.first_us,0) ||
    !meta("delay_end_us",(float)g_sofia_debug.end_us,0) || !meta("case_at_cut_c",points[1].tc,4) || !meta("case_sample_age_ms",(float)case_age_ms,0) || !meta("adc_reference_age_ms",(float)reference_age_ms,0))return false;
 for(unsigned i=0;i<count;i++){
  char v[24],tc[24],tj[24],time[24],line[240];
  if(!format_decimal(v,sizeof(v),points[i].vf,6) || !format_decimal(tc,sizeof(tc),points[i].tc,4) ||
     !format_decimal(tj,sizeof(tj),points[i].tj,4) ||
     !format_decimal(time,sizeof(time),i?((float)(cut_ms-start_ms)/1000+(float)points[i].from_cut_us/1000000):0,6))return false;
  (void)snprintf(line,sizeof(line),"#phase_%u=%s",i,i==0?"BASELINE":i==1?"HOT":"COOLING");if(!sdlog_line(line))return false;
  (void)snprintf(line,sizeof(line),"#cut_us_%u=%lu",i,(unsigned long)points[i].from_cut_us);if(!sdlog_line(line))return false;
  (void)snprintf(line,sizeof(line),"%u,%s,%s,%s,%s,%s",i,tc,tc,v,tj,time);if(!sdlog_line(line))return false;
 }
 return true;
}
job_status_t sofia_step(device_t *d){
 if(status!=JOB_STATUS_RUNNING)return status;
 if(!d || !board_is_ready())return fail(JOB_ERROR_SENSOR);
 uint32_t now=board_millis();d->elapsed_seconds=(now-start_ms)/1000;
 if(!isfinite(d->temperature_c))return fail(JOB_ERROR_SENSOR);
 if(d->temperature_c>=cfg.max_case_c || (g_hw_debug.tc_valid && g_hw_debug.tc_raw_c>=OVERTEMP_C))return fail(JOB_ERROR_OVERTEMP);
 if(!sdlog_ready())return fail(JOB_ERROR_STORAGE);
 d->target_temperature_c=cfg.max_case_c;
 if(g_sofia_debug.phase==1){
  d->progress_percent=(uint8_t)fminf(80,(float)(now-heat_ms)*80/(cfg.heat_s*1000));
  if(!measure && (uint32_t)(now-heat_ms)<cfg.heat_s*1000U)return status;
  /* Voltage under IH is NOT the TSP voltage used with the IM calibration. */
  if(!vf_adc_refresh_reference())return fail(JOB_ERROR_SAMPLE_FAILED);
  float vh;if(!vf_adc_capture(8,&vh) || !isfinite(vh) || vh<=0)return fail(JOB_ERROR_SAMPLE_FAILED);
  g_sofia_debug.heating_v=vh;g_sofia_debug.heating_w=IH_A*vh;
  vf_adc_cancel_samples();sofia_capture_t sample;
  if(!sofia_io_capture(cfg.delay_us,&sample))return fail(JOB_ERROR_SAMPLE_FAILED);
  cut_ms=board_millis();cut_cycles=sample.switch_cycles;
  case_age_ms=cut_ms-g_hw_debug.tc_last_ok_ms;reference_age_ms=g_vf_debug.reference_age_ms;
  float vf=(vf_from_counts(sample.raw,vf_adc_vdda())-THERMAL_ADC_OFFSET_V)/THERMAL_ADC_GAIN;
  float tj=cfg.slope*vf+cfg.intercept;
  if(!isfinite(vf) || !isfinite(tj) || tj<TEMP_MIN_C || tj>=OVERTEMP_C)return fail(JOB_ERROR_OVERTEMP);
  points[count++]=(point_t){vf,d->temperature_c,tj,sample.end_us};
  g_sofia_debug.hot_vf=vf;g_sofia_debug.tj_hot=tj;g_sofia_debug.first_us=sample.first_us;g_sofia_debug.end_us=sample.end_us;
  g_sofia_debug.phase=2;g_sofia_debug.samples=count;d->vf_voltage=vf;return status;
 }
 if(g_sofia_debug.phase==2){
  uint32_t elapsed=sofia_io_elapsed_us(cut_cycles);
  if(elapsed<cooling_us[cooling_index])return status;
  float vf;if(!vf_adc_capture(1,&vf) || !isfinite(vf))return fail(JOB_ERROR_SAMPLE_FAILED);
  uint32_t actual=sofia_io_elapsed_us(cut_cycles);float tj=cfg.slope*vf+cfg.intercept;
  if(!isfinite(tj) || tj<TEMP_MIN_C || tj>=OVERTEMP_C)return fail(JOB_ERROR_OVERTEMP);
  points[count++]=(point_t){vf,d->temperature_c,tj,actual};g_sofia_debug.samples=count;
  /* Skip missed requested instants instead of fabricating duplicate samples. */
  do{cooling_index++;}while(cooling_index<sizeof(cooling_us)/sizeof(cooling_us[0]) && cooling_us[cooling_index]<=actual);
  if(cooling_index<sizeof(cooling_us)/sizeof(cooling_us[0]))return status;
  power_safe();
  if(!flush())return fail(JOB_ERROR_STORAGE);
  bool ok=sdlog_close();open=false;if(!ok)return fail(JOB_ERROR_FINALIZE_FAILED);
  status=JOB_STATUS_COMPLETED;g_sofia_debug.phase=3;d->progress_percent=100;
 }
 return status;
}
