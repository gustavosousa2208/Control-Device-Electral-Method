#include "thermal_tsp.h"
#include <math.h>
bool tsp_slope_valid(float slope)
{ return isfinite(slope) && fabsf(slope)>=1.0f && fabsf(slope)<=100000.0f; }
float tsp_temperature(float slope,float reference_c,float reference_vf,float vf)
{
 if(!tsp_slope_valid(slope)||!isfinite(reference_c)||!isfinite(reference_vf)||!isfinite(vf))return NAN;
 return reference_c+slope*(vf-reference_vf);
}
