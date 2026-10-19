#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "tc_heater.h"
#include "runtime_config.h"
static uint32_t now,sequence;
static bool relay,measuring,allowed=true,ready=true;
static char trace[256];
static void record(char c){size_t n=strlen(trace);assert(n+1<sizeof(trace));trace[n]=c;trace[n+1]=0;}
uint32_t board_millis(void){return now;}
bool board_is_ready(void){return ready;}
void power_safe(void){record('S');relay=false;measuring=true;}
bool power_enable_measurement(void){record('M');measuring=true;return allowed;}
bool power_relay_on(void){record('R');assert(measuring);relay=allowed;return allowed;}
bool power_select_heating(void){record('H');assert(relay);measuring=false;return allowed;}
static void tick(float t){now+=250;tc_heater_service(t,t,true,++sequence,now);}
static void begin(unsigned n){assert(tc_heater_start(n,30,10,25,25,true,now));}
int main(void){
 assert(!tc_heater_start(2,30,10,25,25,true,now)&&!relay);
 begin(3);assert(strstr(trace,"SMRH")&&relay&&!measuring);
 tick(30);assert(g_tc_heat_debug.heating); /* No automatic regulation. */
 tc_heater_advance();assert(g_tc_heat_debug.waiting_next&&!relay&&measuring);
 tick(30);assert(!relay);assert(tc_heater_next()&&relay&&g_tc_heat_debug.target_c==40);
 tc_heater_advance();assert(!relay&&tc_heater_next()&&g_tc_heat_debug.target_c==50);
 tc_heater_advance();assert(!relay&&!g_tc_heat_debug.active);
 begin(4);now+=1001;tc_heater_service(25,25,true,sequence,now-1001);assert(!relay&&g_tc_heat_debug.error==2);
 begin(3);tick(41);assert(!relay&&g_tc_heat_debug.error==3);
 begin(3);now+=TC_HEAT_POINT_TIMEOUT_MS;tc_heater_service(25,25,true,++sequence,now);assert(!relay&&g_tc_heat_debug.error==4);
 allowed=false;assert(!tc_heater_start(3,30,10,25,25,true,now)&&!relay&&g_tc_heat_debug.error==5);allowed=true;
 begin(3);ready=false;tick(25);assert(!relay&&g_tc_heat_debug.error==2);ready=true;
 begin(3);tick(NAN);assert(!relay&&g_tc_heat_debug.error==2);
 begin(3);tc_heater_stop();assert(!relay&&measuring);
 puts("TC manual confirmation: full heat, off between points, manual next, sensor/timeout/temperature guards PASS");
}
