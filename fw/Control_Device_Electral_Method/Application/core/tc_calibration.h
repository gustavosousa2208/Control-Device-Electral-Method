#ifndef TC_CALIBRATION_H
#define TC_CALIBRATION_H
#include <stdbool.h>
#include <stddef.h>
#define TC_OFFSET_LIMIT_C 20.0f
#define TC_CAL_MIN_SPAN_C 5.0f
#define TC_CAL_MIN_R2 0.98f
#define TC_CAPTURE_SAMPLES 16U
#define TC_CAPTURE_MAX_SPAN_C 1.0f
typedef struct { float gain, offset, alpha, r2; unsigned points; } tc_cal_params_t;
void tc_calibration_init(void);
bool tc_calibration_valid(const tc_cal_params_t *params);
bool tc_calibration_set(const tc_cal_params_t *params);
const tc_cal_params_t *tc_calibration_get(void);
float tc_calibration_offset(void);
float tc_calibration_apply(float raw);
bool tc_calibration_fit(const float *raw, const float *reference, size_t count, tc_cal_params_t *out);
#endif
