#include "tc_calibration.h"
#include "runtime_config.h"
#include <math.h>
static tc_cal_params_t active={1,0,THERMAL_DISPLAY_FILTER_ALPHA,1,0};
void tc_calibration_init(void) { active=(tc_cal_params_t){1,0,THERMAL_DISPLAY_FILTER_ALPHA,1,0}; }
bool tc_calibration_valid(const tc_cal_params_t *p) {
 return p && isfinite(p->gain) && p->gain>=0.5f && p->gain<=1.5f &&
 isfinite(p->offset) && fabsf(p->offset)<=TC_OFFSET_LIMIT_C &&
 isfinite(p->alpha) && p->alpha>=0.05f && p->alpha<=1 &&
 isfinite(p->r2) && p->r2>=TC_CAL_MIN_R2 && p->r2<=1 &&
 (p->points==0 || p->points==3 || p->points==4);
}
bool tc_calibration_set(const tc_cal_params_t *p) { if (!tc_calibration_valid(p)) return false;active=*p;return true; }
const tc_cal_params_t *tc_calibration_get(void) { return &active; }
float tc_calibration_offset(void) { return active.offset; }
float tc_calibration_apply(float raw) { return isfinite(raw)?active.gain*raw+active.offset:NAN; }
bool tc_calibration_fit(const float *x,const float *y,size_t n,tc_cal_params_t *out) {
 if (!x || !y || !out || (n!=3 && n!=4)) return false;
 double mx=0,my=0,xx=0,yy=0,xy=0;
 float low=x[0],high=x[0];
 for(size_t i=0;i<n;i++) {
  if (!isfinite(x[i]) || !isfinite(y[i]) || y[i]<TEMP_MIN_C || y[i]>TEMP_MAX_C) return false;
  for(size_t j=0;j<i;j++) if (fabsf(x[i]-x[j])<0.5f || fabsf(y[i]-y[j])<0.5f) return false;
  if (x[i]<low) low=x[i];
  if (x[i]>high) high=x[i];
  mx+=x[i];my+=y[i];
 }
 if (high-low<TC_CAL_MIN_SPAN_C) return false;
 mx/=n;my/=n;
 for(size_t i=0;i<n;i++){double dx=x[i]-mx,dy=y[i]-my;xx+=dx*dx;yy+=dy*dy;xy+=dx*dy;}
 if (xx<=0 || yy<=0) return false;
 double r2=xy*xy/(xx*yy);if(r2>1)r2=1;
 tc_cal_params_t p={(float)(xy/xx),(float)(my-(xy/xx)*mx),active.alpha,(float)r2,(unsigned)n};
 if (!tc_calibration_valid(&p)) return false;
 *out=p;return true;
}
