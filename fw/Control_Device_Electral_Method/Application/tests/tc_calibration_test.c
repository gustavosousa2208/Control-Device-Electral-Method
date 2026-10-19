#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "tc_calibration.h"
int main(void) {
 tc_calibration_init();assert(tc_calibration_apply(25)==25);
 float raw[4]={20,40,60,80},ref[4]={21,41.4f,61.8f,82.2f};tc_cal_params_t p;
 assert(tc_calibration_fit(raw,ref,3,&p));assert(fabsf(p.gain-1.02f)<0.0001f && fabsf(p.offset-0.6f)<0.001f);
 assert(tc_calibration_fit(raw,ref,4,&p));assert(tc_calibration_set(&p));assert(fabsf(tc_calibration_apply(40)-41.4f)<0.001f);
 assert(isnan(tc_calibration_apply(NAN)));
 float identical[4]={20,20,20,20};assert(!tc_calibration_fit(identical,ref,4,&p));
 float short_span[3]={20,21,22};assert(!tc_calibration_fit(short_span,ref,3,&p));
 ref[2]=10;assert(!tc_calibration_fit(raw,ref,3,&p));
 p=*tc_calibration_get();p.gain=-1;assert(!tc_calibration_set(&p));
 p=*tc_calibration_get();p.offset=21;assert(!tc_calibration_set(&p));
 p=*tc_calibration_get();p.alpha=0;assert(!tc_calibration_set(&p));
 p=*tc_calibration_get();p.alpha=1;assert(tc_calibration_set(&p));
 puts("TC 3/4 point fit, affine correction and invalid profiles: PASS");return 0;
}
