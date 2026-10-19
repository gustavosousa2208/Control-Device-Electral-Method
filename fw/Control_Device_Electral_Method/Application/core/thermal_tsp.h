#ifndef THERMAL_TSP_H
#define THERMAL_TSP_H
#include <stdbool.h>
/* Signed dT/dVf in degC/V, NOT positive |K| and NOT mV/degC. */
bool tsp_slope_valid(float slope_c_per_v);
float tsp_temperature(float slope_c_per_v,float reference_c,float reference_vf,float vf);
#endif
