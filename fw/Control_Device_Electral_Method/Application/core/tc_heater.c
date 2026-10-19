#include "tc_heater.h"
#include "board.h"
#include "power.h"
#include "runtime_config.h"
#include <math.h>
volatile tc_heat_debug_t g_tc_heat_debug;
static float first_c,step_c;
static uint32_t point_ms;
void tc_heater_stop(void) {
 power_safe();g_tc_heat_debug.active=false;g_tc_heat_debug.heating=false;
 g_tc_heat_debug.waiting_next=false;
}
static bool fail(unsigned error) {
 tc_heater_stop();g_tc_heat_debug.error=error;return false;
}
static bool sensor_ok(float t,float raw,bool valid,uint32_t stamp) {
 return valid && isfinite(t) && isfinite(raw) && t>=TEMP_MIN_C && raw>=TEMP_MIN_C &&
  (uint32_t)(board_millis()-stamp)<=TC_HEAT_SENSOR_MAX_AGE_MS;
}
static bool energize(void) {
 if(!power_enable_measurement() || !power_relay_on() || !power_select_heating())return fail(5);
 g_tc_heat_debug.heating=true;point_ms=board_millis();return true;
}
bool tc_heater_start(unsigned points,float first,float step,float t,float raw,bool valid,uint32_t stamp) {
 tc_heater_stop();g_tc_heat_debug=(tc_heat_debug_t){0};
 if((points!=3 && points!=4) || !isfinite(first) || !isfinite(step) ||
    first<TC_HEAT_MIN_C || step<TC_HEAT_MIN_STEP_C || first+(points-1)*step>TC_HEAT_MAX_C)return fail(1);
 if(!sensor_ok(t,raw,valid,stamp) || !board_is_ready())return fail(2);
 if(t>=OVERTEMP_C || raw>=OVERTEMP_C || t>first+TC_HEAT_OVERSHOOT_C)return fail(3);
 first_c=first;step_c=step;g_tc_heat_debug.target_c=first;
 g_tc_heat_debug.temperature_c=t;g_tc_heat_debug.points=points;g_tc_heat_debug.point=1;
 if(!energize())return false;
 g_tc_heat_debug.active=true;return true;
}
void tc_heater_service(float t,float raw,bool valid,uint32_t sequence,uint32_t stamp) {
 (void)sequence;
 if(!g_tc_heat_debug.active || g_tc_heat_debug.waiting_next)return;
 if(!board_is_ready() || !sensor_ok(t,raw,valid,stamp)){(void)fail(2);return;}
 g_tc_heat_debug.temperature_c=t;
 if(t>=OVERTEMP_C || raw>=OVERTEMP_C || t>g_tc_heat_debug.target_c+TC_HEAT_OVERSHOOT_C){(void)fail(3);return;}
 if((uint32_t)(board_millis()-point_ms)>=TC_HEAT_POINT_TIMEOUT_MS)(void)fail(4);
 /* No temperature regulator: operator confirms the digital reference target. */
}
bool tc_heater_next(void) {
 if(!g_tc_heat_debug.active || !g_tc_heat_debug.waiting_next || !board_is_ready())return false;
 g_tc_heat_debug.point++;g_tc_heat_debug.target_c=first_c+(g_tc_heat_debug.point-1)*step_c;
 g_tc_heat_debug.waiting_next=false;
 return energize();
}
void tc_heater_advance(void) {
 if(!g_tc_heat_debug.active)return;
 power_safe();g_tc_heat_debug.heating=false;
 if(g_tc_heat_debug.point>=g_tc_heat_debug.points){tc_heater_stop();return;}
 g_tc_heat_debug.waiting_next=true;
}
