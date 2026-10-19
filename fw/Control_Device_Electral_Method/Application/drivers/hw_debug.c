#include "hw_debug.h"
#include "runtime_config.h"
volatile hw_debug_t g_hw_debug;
volatile hw_tuning_t g_hw_tuning = {
 THERMAL_ENCODER_COUNTS_PER_DETENT, THERMAL_ENCODER_DIRECTION,
 THERMAL_BUTTON_DEBOUNCE_MS, THERMAL_DISPLAY_FILTER_ALPHA
};
