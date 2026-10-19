#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "stm32f4xx_hal.h"
#include "vf_adc.h"
volatile vf_adc_debug_t g_vf_debug;
static ADC_TypeDef adc;
ADC_HandleTypeDef hadc1={.Instance=&adc};
DWT_Type mock_dwt;
uint32_t SystemCoreClock=168000000;
static unsigned starts,stops,reads;
static unsigned mode;
bool DWT_US_Delay(uint32_t n){assert(n==0);return mode!=4;}
void board_adc_cancel(void){stops++;adc.SR=0;adc.CR2=0;}
bool board_adc_begin(bool internal){assert(!internal);starts++;adc.CR2=ADC_CR2_SWSTART;return mode!=5;}
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *h){reads++;uint32_t n=h->Instance->DR;h->Instance->SR=0;return n;}
void mock_nop(void){
 mock_dwt.CYCCNT+=mode==6?80000U:200U;
 if(mode!=1 && (adc.CR2&ADC_CR2_SWSTART)) {
  adc.CR2&=~ADC_CR2_SWSTART;adc.DR=mode==2?0:mode==3?4095:744;adc.SR|=ADC_SR_EOC;
 }
}
#include "adc_burst_under_test.inc"
static void reset(unsigned m){mode=m;starts=stops=reads=0;adc=(ADC_TypeDef){0};mock_dwt.CYCCNT=0;}
int main(void){float out;
 reset(0);assert(board_adc_burst(64,&out));assert(out==744&&starts==1&&reads==64&&stops==2&&g_vf_debug.completed_samples==64&&g_vf_debug.burst_status==0);
 reset(0);mock_dwt.CYCCNT=UINT32_MAX-500;assert(board_adc_burst(8,&out)&&reads==8);
 reset(1);assert(!board_adc_burst(64,&out)&&g_vf_debug.burst_status==4&&stops==2);
 reset(2);assert(!board_adc_burst(64,&out)&&g_vf_debug.burst_status==5&&stops==2);
 reset(3);assert(!board_adc_burst(64,&out)&&g_vf_debug.burst_status==6&&stops==2);
 reset(4);assert(!board_adc_burst(64,&out)&&g_vf_debug.burst_status==2&&starts==0);
 reset(5);assert(!board_adc_burst(64,&out)&&g_vf_debug.burst_status==3&&stops==2);
 reset(6);assert(!board_adc_burst(64,&out)&&g_vf_debug.burst_status==7&&stops==2);
 reset(0);assert(!board_adc_burst(0,&out)&&g_vf_debug.burst_status==1&&starts==0);
 puts("Actual ADC burst adapter: one startup, 64/8 conversions, wrap, timeout, zero, saturation and cleanup PASS");
}
