#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "app.h"
#include "ui.h"
#include "job.h"
#include "board.h"
#include "oled.h"
#include "tc_calibration.h"
#include "hw_debug.h"
#include "config.h"
static uint32_t now;
static input_event_t pending;
static float raw=20;
static bool save_ok=true,has_saved;
static tc_cal_params_t persisted;
static cal_params_t dut={-357,279,1,3,true};
static unsigned writes,unsafe;
static bool relay,measuring;
uint32_t board_millis(void){return now;}
bool board_is_ready(void){return true;}
bool board_power_control_enabled(void){return true;}
void board_force_safe(void){}
void board_debug_service(void){}
bool board_sd_prepare(void){return true;}
void power_init(void){}
void power_safe(void){relay=false;measuring=true;}
bool power_enable_measurement(void){measuring=true;return true;}
bool power_relay_on(void){assert(measuring);relay=true;return true;}
bool power_select_heating(void){assert(relay);measuring=false;return true;}
void rtd_init(void){g_hw_debug.tc_valid=false;}
bool rtd_read_temperature(float *v){
 g_hw_debug.tc_raw_c=raw;g_hw_debug.tc_valid=true;g_hw_debug.tc_samples++;g_hw_debug.tc_last_ok_ms=now;
 *v=tc_calibration_apply(raw);return true;
}
void vf_adc_init(void){}
void vf_adc_service(void){}
bool vf_adc_read_voltage(float *v){*v=0.6f;return true;}
float vf_adc_vdda(void){return 3.3f;}
bool vf_adc_vdda_fallback(void){return false;}
void oled_init(void){}
bool oled_ready(void){return true;}
bool oled_busy(void){return true;}
void oled_clear(void){}
void oled_text(int x,int y,const char *s){(void)x;(void)y;(void)s;}
void oled_text_big(int x,int y,const char *s){(void)x;(void)y;(void)s;}
void oled_rect(int x,int y,int w,int h,bool b){(void)x;(void)y;(void)w;(void)h;(void)b;}
void oled_bitmap(int x,int y,unsigned w,unsigned h,const uint8_t *d,size_t n){(void)x;(void)y;(void)w;(void)h;(void)d;(void)n;}
void oled_xbm(int x,int y,unsigned w,unsigned h,const uint8_t *d,size_t n){(void)x;(void)y;(void)w;(void)h;(void)d;(void)n;}
bool oled_present(void){return true;}
void oled_service(void){}
void oled_recover_service(void){}
void encoder_init(void){}
input_event_t encoder_get_event(void){input_event_t e=pending;pending=INPUT_EVENT_NONE;return e;}
void sdlog_init(void){}
bool sdlog_ready(void){return true;}
bool sdlog_save_tc_calibration(const tc_cal_params_t *p){writes++;if(!save_ok)return false;persisted=*p;has_saved=true;return true;}
bool sdlog_load_tc_calibration(tc_cal_params_t *p){if(!has_saved)return false;*p=persisted;return true;}
bool sdlog_reset_tc_calibration(void){
 if(!save_ok)return false;
 persisted=(tc_cal_params_t){1,0,0.25f,1,0};has_saved=true;writes+=2;return true;
}
void calibration_init(void){}
const cal_params_t *calibration_get(void){return &dut;}
void calibration_set(const cal_params_t *p){dut=*p;}
void job_init(void){}
bool job_start_calibration_params(float a,float b,float c){(void)a;(void)b;(void)c;unsafe++;return false;}
bool job_start_test(float a,float b,float c){(void)a;(void)b;(void)c;unsafe++;return false;}
bool job_start_test_manual_k(float a,float b,float c,float k,float t){(void)k;(void)t;return job_start_test(a,b,c);}
void job_abort(void){}
job_error_t job_error(void){return JOB_ERROR_NONE;}
job_status_t job_step(device_t *d){(void)d;return JOB_STATUS_IDLE;}
static void step(input_event_t e){now+=250;pending=e;app_run();}
static void open_heat(bool four){
 step(INPUT_EVENT_CLICK);step(INPUT_EVENT_CLICK);
 step(INPUT_EVENT_CW);step(INPUT_EVENT_CW);step(INPUT_EVENT_CLICK);
 step(INPUT_EVENT_CW);step(INPUT_EVENT_CW);step(INPUT_EVENT_CLICK);
 assert(ui_current_screen()==UI_SCREEN_TC_HEAT_SETUP);
 if(four){step(INPUT_EVENT_CLICK);step(INPUT_EVENT_CW);step(INPUT_EVENT_CLICK);}
 step(INPUT_EVENT_CW);step(INPUT_EVENT_CW);step(INPUT_EVENT_CW);step(INPUT_EVENT_CLICK);
 assert(ui_current_screen()==UI_SCREEN_TC_MULTI && app_device()->tc_heat_active && relay);
}
int main(void){
 app_init();open_heat(false);assert(!unsafe);step(INPUT_EVENT_LONG);assert(!relay&&!app_device()->tc_heat_active);
 raw=25;app_init();open_heat(true);
 assert(app_device()->tc_required==4);
 for(unsigned point=0;point<4;point++) {
  /* Local thermocouple is 2 C low. Operator sees DIGITAL at preset target. */
  raw=28+10*(float)point;step(INPUT_EVENT_NONE);
  step(INPUT_EVENT_CLICK); /* CONFIRMAR ALVO already selected, no REF entry. */
  assert(!app_device()->tc_capture_busy&&app_device()->tc_capture_count==1);
  assert(app_device()->tc_points==point+1&&!relay);
  if(point<3){assert(app_device()->tc_heat_waiting_next);step(INPUT_EVENT_CLICK);assert(relay);}
 }
 assert(!relay&&!app_device()->tc_heat_active&&app_device()->tc_candidate_valid&&writes==0);
 step(INPUT_EVENT_CW);step(INPUT_EVENT_CLICK);
 assert(app_device()->tc_adjust_status==1&&writes==1&&persisted.points==4&&fabsf(persisted.gain-1)<.001f&&fabsf(persisted.offset-2)<.001f);
 assert(!unsafe&&!relay);
 puts("TC confirm UI: one click per preset digital target, immediate raw sample, power off, no REF/PID, four points and saved fit PASS");
}

volatile sofia_debug_t g_sofia_debug;
void sofia_init(void){}
bool sofia_start(const sofia_config_t *c,const device_t *d){(void)c;(void)d;return false;}
job_status_t sofia_step(device_t *d){(void)d;return JOB_STATUS_IDLE;}
void sofia_measure_now(void){}
void sofia_abort(void){}
