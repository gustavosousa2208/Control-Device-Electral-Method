#ifndef TC_HEATER_H
#define TC_HEATER_H
#include <stdbool.h>
#include <stdint.h>
/* Live Expressions: g_tc_heat_debug. Error: 1 params, 2 sensor, 3 overtemp,
 * 4 point timeout, 5 power interlock. No automatic restart after a fault. */
typedef struct {
 bool active,heating,waiting_next;
 unsigned point,points,error;
 float target_c,temperature_c;
} tc_heat_debug_t;
extern volatile tc_heat_debug_t g_tc_heat_debug;
bool tc_heater_start(unsigned points,float first,float step,float temperature,float raw,bool valid,uint32_t sample_ms);
void tc_heater_service(float temperature,float raw,bool valid,uint32_t sequence,uint32_t sample_ms);
bool tc_heater_next(void);
void tc_heater_advance(void);
void tc_heater_stop(void);
#endif
