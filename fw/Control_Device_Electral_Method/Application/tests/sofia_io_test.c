#include <assert.h>
#include <stdio.h>
#include "stm32f4xx_hal.h"
#include "sofia_io.h"
static ADC_TypeDef adc;
ADC_HandleTypeDef hadc1={.Instance=&adc};
DWT_Type mock_dwt;
uint32_t SystemCoreClock=168000000;
static unsigned reads;static bool measurement,timeout;
bool board_is_ready(void){return true;}
void board_adc_cancel(void){adc.SR=adc.CR2=0;}
bool board_adc_begin(bool internal){assert(!internal);adc.CR2=ADC_CR2_SWSTART;return true;}
bool DWT_US_Delay(uint32_t us){mock_dwt.CYCCNT+=168*us;return true;}
bool power_enable_measurement(void){measurement=true;return true;}
void mock_nop(void){mock_dwt.CYCCNT+=168;if(!timeout && (adc.CR2&ADC_CR2_SWSTART)){adc.CR2=0;adc.SR=ADC_SR_EOC;adc.DR=measurement?744:3000;}}
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *h){reads++;h->Instance->SR=0;return h->Instance->DR;}
int main(void){sofia_capture_t out;
 assert(sofia_io_capture(20,&out)&&measurement&&reads==2&&out.raw==744&&out.first_us>=20&&out.end_us>out.first_us);
 timeout=true;assert(!sofia_io_capture(20,&out)&&adc.CR2==0);
 assert(!sofia_io_capture(1,&out));puts("Sofia ADC timing: discard heating conversion, fresh measurement after 20 us, deadline cleanup PASS");}
