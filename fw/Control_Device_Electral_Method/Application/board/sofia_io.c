#include "sofia_io.h"
#include "stm32f4xx_hal.h"
#include "board.h"
#include "peripheral_io.h"
#include "dwt_us.h"
#include "power.h"
extern ADC_HandleTypeDef hadc1;
uint32_t sofia_io_elapsed_us(uint32_t start) {
 if(!SystemCoreClock)return UINT32_MAX;
 return (uint32_t)(((uint64_t)(uint32_t)(DWT->CYCCNT-start)*1000000U)/SystemCoreClock);
}
static bool completed(uint32_t start) {
 unsigned guard=100000U;
 while(!(hadc1.Instance->SR&ADC_SR_EOC)) {
  if(--guard==0 || sofia_io_elapsed_us(start)>=400U)return false;
  __NOP();
 }
 return sofia_io_elapsed_us(start)<400U;
}
bool sofia_io_capture(unsigned delay_us,sofia_capture_t *out) {
 if(!out || delay_us<5 || delay_us>1000 || !board_is_ready() || !DWT_US_Delay(0))return false;
 *out=(sofia_capture_t){0};board_adc_cancel();
 /* Prepare and warm ADC under heating; this conversion is discarded.
  * The first recorded conversion starts only after SELECT and settling. */
 uint32_t prepare=DWT->CYCCNT;
 if(!board_adc_begin(false) || !completed(prepare)){board_adc_cancel();return false;}
 (void)HAL_ADC_GetValue(&hadc1);
 if(!power_enable_measurement()){board_adc_cancel();return false;}
 out->switch_cycles=DWT->CYCCNT; /* GPIO software boundary, not physical MOSFET timing. */
 if(!DWT_US_Delay(delay_us)){board_adc_cancel();return false;}
 out->first_us=sofia_io_elapsed_us(out->switch_cycles);
 hadc1.Instance->CR2|=ADC_CR2_SWSTART;
 uint32_t conversion=DWT->CYCCNT;
 if(!completed(conversion)){board_adc_cancel();return false;}
 uint32_t raw=HAL_ADC_GetValue(&hadc1);out->end_us=sofia_io_elapsed_us(out->switch_cycles);
 board_adc_cancel();
 if(raw==0 || raw>=4095)return false;
 out->raw=(float)raw;return true;
}
