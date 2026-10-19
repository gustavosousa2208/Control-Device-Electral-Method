#ifndef SOFIA_H
#define SOFIA_H
#include "job.h"
typedef struct {float slope,intercept,max_case_c;unsigned heat_s,delay_us;} sofia_config_t;
typedef struct {unsigned phase,error,samples;float hot_vf,heating_v,heating_w,tj_hot;uint32_t first_us,end_us;} sofia_debug_t;
extern volatile sofia_debug_t g_sofia_debug;
void sofia_init(void);
bool sofia_start(const sofia_config_t *cfg,const device_t *device);
job_status_t sofia_step(device_t *device);
void sofia_measure_now(void);
void sofia_abort(void);
#endif
