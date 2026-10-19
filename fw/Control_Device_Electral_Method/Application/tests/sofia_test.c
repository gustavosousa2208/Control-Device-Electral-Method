#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "sofia.h"
#include "sofia_io.h"
#include "board.h"
#include "vf_adc.h"
volatile vf_adc_debug_t g_vf_debug;
static uint32_t now,cut;
static bool relay,measurement,storage=true,adc=true,capture=true;
static char logdata[14000];
uint32_t board_millis(void){return now;}
bool board_is_ready(void){return true;}
void board_delay_us(uint32_t us){assert(us==30);}
void power_safe(void){measurement=true;relay=false;}
bool power_enable_measurement(void){measurement=true;return true;}
bool power_relay_on(void){assert(measurement);relay=true;return true;}
bool power_select_heating(void){assert(relay);measurement=false;return true;}
bool vf_adc_ready(void){return adc;}
bool vf_adc_refresh_reference(void){assert(relay);return adc;}
void vf_adc_cancel_samples(void){}
float vf_adc_vdda(void){return 3.3f;}
bool vf_adc_capture(unsigned n,float *out){assert(n==8 || n==1);if(!adc)return false;*out=measurement?(n==8?.55f:.51f):.95f;return true;}
bool sofia_io_capture(unsigned delay,sofia_capture_t *out){assert(delay==20&&!measurement);if(!capture)return false;measurement=true;cut=now;*out=(sofia_capture_t){.raw=.50f/3.3f*4095,.switch_cycles=now,.first_us=21,.end_us=23};return true;}
uint32_t sofia_io_elapsed_us(uint32_t start){return (now-start)*1000;}
bool sdlog_ready(void){return storage;}
bool sdlog_open_run(bool cal){assert(!cal);return storage;}
bool sdlog_line(const char *line){if(!storage)return false;assert(strlen(logdata)+strlen(line)+2<sizeof(logdata));strcat(logdata,line);strcat(logdata,"\n");return true;}
bool sdlog_close(void){return storage;}
static sofia_config_t config={-467,279.36f,85,1,20};
static device_t d={.temperature_c=25};
static void reset(void){now=0;relay=false;measurement=true;storage=adc=capture=true;logdata[0]=0;d.temperature_c=25;sofia_init();}
int main(void){
 reset();assert(sofia_start(&config,&d)&&relay&&!measurement);now=1000;
 assert(sofia_step(&d)==JOB_STATUS_RUNNING&&measurement&&relay&&g_sofia_debug.samples==2);
 assert(fabsf(g_sofia_debug.heating_w-.758f*.95f)<.00001f&&fabsf(g_sofia_debug.tj_hot-45.86f)<.01f);
 assert(!strstr(logdata,"#phase_")); /* SD writes deferred past capture/cooling. */
 for(unsigned ms=1;ms<=5000;ms++){now=cut+ms;sofia_step(&d);}
 assert(sofia_step(&d)==JOB_STATUS_COMPLETED&&!relay&&g_sofia_debug.samples==14);
 assert(strstr(logdata,"#mode=TSP_STEP")&&strstr(logdata,"#delay_first_us=21")&&strstr(logdata,"#current_heating_a=0.758000"));
 assert(strstr(logdata,"#phase_1=HOT")&&strstr(logdata,"#cut_us_13=5000000"));
 reset();assert(sofia_start(&config,&d));sofia_abort();assert(!relay&&sofia_step(&d)==JOB_STATUS_ABORTED);
 reset();storage=false;assert(!sofia_start(&config,&d)&&!relay);
 reset();adc=false;assert(!sofia_start(&config,&d)&&!relay);
 reset();assert(sofia_start(&config,&d));capture=false;sofia_measure_now();assert(sofia_step(&d)==JOB_STATUS_FAILED&&!relay);
 reset();assert(sofia_start(&config,&d));d.temperature_c=85;assert(sofia_step(&d)==JOB_STATUS_FAILED&&!relay);
 reset();assert(sofia_start(&config,&d));now=100;sofia_measure_now();assert(sofia_step(&d)==JOB_STATUS_RUNNING&&g_sofia_debug.phase==2);sofia_abort();
 puts("Sofia independent step: 758/2 mA metadata, full calibration equation, power voltage, hot/cooling timing, deferred writes and safe failures PASS");
}
