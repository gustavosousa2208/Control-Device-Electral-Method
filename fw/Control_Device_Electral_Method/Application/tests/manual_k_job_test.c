#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "job.h"
#include "thermal_tsp.h"
#include "config.h"
#include "calibration.h"
#include "power.h"
#include "sdlog.h"
#include "vf_adc.h"
#include "board.h"
static cal_params_t stored;
static bool relay,measuring,storage=true,adc=true,write_ok=true;
static unsigned saves,closed;
static float vf=0.65f;
static uint32_t now;
static char trace[2048],log_text[4096];
static void record(char c){size_t n=strlen(trace);assert(n+1<sizeof(trace));trace[n]=c;trace[n+1]=0;}
uint32_t board_millis(void){return now;}
bool board_is_ready(void){return true;}
void board_delay_us(uint32_t n){assert(n==30);record('D');}
void power_safe(void){relay=false;measuring=true;record('S');}
bool power_enable_measurement(void){measuring=true;record('M');return true;}
bool power_relay_on(void){assert(measuring);relay=true;record('R');return true;}
bool power_select_heating(void){assert(relay);measuring=false;record('H');return true;}
bool vf_adc_ready(void){return true;}
void vf_adc_cancel_samples(void){}
bool vf_adc_capture(unsigned count,float *out){assert(measuring);assert(count==(job_kind()==JOB_KIND_CALIBRATION?CAL_VF_SAMPLES:TEST_VF_SAMPLES));record('V');*out=adc?vf:NAN;return adc;}
bool sdlog_ready(void){return storage;}
bool sdlog_open_run(bool cal){(void)cal;record('O');return storage;}
bool sdlog_line(const char *s){record('W');if(!write_ok)return false;assert(strlen(log_text)+strlen(s)+2<sizeof(log_text));strcat(log_text,s);strcat(log_text,"\n");return true;}
bool sdlog_close(void){closed++;return true;}
bool sdlog_save_calibration(const cal_params_t *c){stored=*c;saves++;return true;}
const cal_params_t *calibration_get(void){return &stored;}
void calibration_set(const cal_params_t *c){stored=*c;}
static void reset(void){trace[0]=log_text[0]=0;relay=false;measuring=true;storage=adc=write_ok=true;saves=closed=0;vf=.65f;now=0;stored=(cal_params_t){0};job_init();}
static job_status_t tick(device_t *d,float temp){now+=100;d->temperature_c=temp;d->sensor_sequence++;return job_step(d);}
int main(void){
 assert(REQUIRE_CAL==1);reset();
 assert(!job_start_test(30,40,10)&&job_error()==JOB_ERROR_NO_CALIBRATION&&!trace[0]);
 assert(!job_start_test_manual_k(30,40,10,0,25)&&!trace[0]);
 assert(!job_start_test_manual_k(30,40,10,NAN,25)&&!trace[0]);
 assert(!job_start_test_manual_k(30,40,10,-500,NAN)&&!trace[0]);
 assert(tsp_slope_valid(-500)&&tsp_slope_valid(500)&&!tsp_slope_valid(INFINITY));
 assert(fabsf(tsp_temperature(-500,25,.65f,.60f)-50)<.001f);
 reset();assert(job_start_test_manual_k(30,40,10,-500,25));
 assert(strstr(trace,"OMRDVWH")&&g_manual_k_debug.reference_valid&&!stored.valid&&saves==0);
 assert(strstr(log_text,"#mode=MANUAL_K")&&strstr(log_text,"k_dtdvf_c_per_v=-500.0000"));
 device_t d={0};vf=.64f;tick(&d,30);tick(&d,30);
 assert(strstr(log_text,",0.64000,30.00,")&&!stored.valid);
 vf=.62f;tick(&d,40);assert(tick(&d,40)==JOB_STATUS_COMPLETED);
 assert(!relay&&saves==0&&closed==1&&!stored.valid);
 reset();adc=false;assert(!job_start_test_manual_k(30,40,10,-500,25));assert(!relay&&!strchr(trace,'H')&&job_error()==JOB_ERROR_SAMPLE_FAILED);
 reset();write_ok=false;assert(!job_start_test_manual_k(30,40,10,-500,25));assert(!relay&&!strchr(trace,'H')&&job_error()==JOB_ERROR_STORAGE);
 reset();assert(job_start_test_manual_k(30,40,10,-500,25));vf=.20f;d=(device_t){0};
 assert(tick(&d,30)==JOB_STATUS_FAILED&&job_error()==JOB_ERROR_OVERTEMP&&!relay);
 reset();assert(job_start_test_manual_k(30,40,10,-500,25));d=(device_t){0};tick(&d,NAN);assert(!relay);job_abort();
 reset();assert(job_start_calibration_params(30,50,10));assert(!strchr(trace,'H')&&!strchr(log_text,'#'));job_abort();
 puts("Manual signed K: cold anchor before heat, REQUIRE_CAL bypass only explicitly, SD/ADC guards, inferred Tj overtemp; external calibration preserved PASS");
}
