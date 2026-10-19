#ifndef HW_DEBUG_H
#define HW_DEBUG_H
#include <stdint.h>
#include <stdbool.h>
typedef struct {
 uint32_t sd_attempts, sd_phase, sd_init_status, sd_wide_status, sd_hal_error, sd_bus_bits, sd_card_state;
 int32_t sd_mount_result;
 uint32_t oled_prepare_attempts, oled_write_errors, oled_last_hal_status;
 uint32_t updated_ms, sequence;
 bool board_ready, power_enabled;
 uint32_t gpio_a_idr, gpio_b_idr, gpio_c_idr, gpio_d_idr, gpio_e_idr;
 uint32_t gpio_a_odr, gpio_b_odr, gpio_c_odr, gpio_d_odr, gpio_e_odr;
 bool relay_level, select_level, tc_cs_level, encoder_a, encoder_b, encoder_button_pressed;
 bool oled_scl, oled_sda;
 uint32_t encoder_counter, encoder_last_counter;
 int32_t encoder_delta, encoder_remainder;
 uint32_t encoder_cw, encoder_ccw, encoder_clicks, encoder_long, encoder_rejected_jumps;
 int32_t encoder_event;
 uint32_t tc_frame, tc_samples, tc_errors, tc_last_ok_ms;
 int32_t tc_status;
 bool tc_valid;
 float tc_raw_c, tc_internal_c, tc_display_c, tc_min_c, tc_max_c;
 float vf_v, vdda_v;
 bool oled_ready, storage_ready;
 float tc_offset_c, tc_corrected_c, tc_gain, tc_display_corrected_c;
} hw_debug_t;
typedef struct {
 uint32_t encoder_counts_per_detent;
 int32_t encoder_direction;
 uint32_t button_debounce_ms;
 float display_filter_alpha;
} hw_tuning_t;
extern volatile hw_debug_t g_hw_debug;
extern volatile hw_tuning_t g_hw_tuning;
void board_debug_service(void);
#endif
