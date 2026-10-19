#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "encoder.h"
#include "max31855.h"
#include "hw_debug.h"
static uint32_t now, counter, frame = 100U<<18;
static bool pressed, spi_ok=true;
uint32_t board_millis(void){return now;}
bool board_encoder_start(uint32_t *p){*p=counter;return true;}
uint32_t board_encoder_counter(void){return counter;}
bool board_encoder_button_pressed(void){return pressed;}
bool board_max31855_read32(uint32_t *p){*p=frame;return spi_ok;}
static input_event_t step(int delta,unsigned ms){counter+=(uint32_t)delta;now+=ms;return encoder_get_event();}
int main(void){
 g_hw_tuning.encoder_direction=1;g_hw_tuning.encoder_counts_per_detent=2;encoder_init();
 assert(step(1,500)==INPUT_EVENT_NONE);assert(step(0,2000)==INPUT_EVENT_NONE);
 assert(step(1,500)==INPUT_EVENT_CW); /* Slow partial steps never expire. */
 assert(step(-1,500)==INPUT_EVENT_NONE);assert(step(-1,500)==INPUT_EVENT_CCW);
 assert(step(1,1)==INPUT_EVENT_NONE);assert(step(-1,1)==INPUT_EVENT_NONE);
 g_hw_tuning.encoder_counts_per_detent=4;encoder_init();
 for(int i=0;i<3;i++)assert(step(1,500)==INPUT_EVENT_NONE);
 assert(step(1,500)==INPUT_EVENT_CW);
 counter=UINT32_MAX;encoder_init();assert(step(4,100)==INPUT_EVENT_CW);
 g_hw_tuning.encoder_direction=-1;assert(step(4,100)==INPUT_EVENT_CCW);
 assert(step(500,1)==INPUT_EVENT_NONE);assert(g_hw_debug.encoder_rejected_jumps==1);
 pressed=true;step(0,1);step(0,5);pressed=false;assert(step(0,1)==INPUT_EVENT_NONE);
 pressed=true;step(0,30);step(0,30);pressed=false;step(0,30);assert(step(0,30)==INPUT_EVENT_CLICK);
 max31855_init();float value;assert(max31855_read(&value)==MAX31855_OK);
 assert(value==25 && g_hw_debug.tc_display_c==25);
 frame=116U<<18;now+=250;assert(max31855_read(&value)==MAX31855_OK);
 assert(value==29 && g_hw_debug.tc_raw_c==29 && g_hw_debug.tc_display_c==26);
 assert(g_hw_debug.tc_min_c==25 && g_hw_debug.tc_max_c==29);
 frame=(1U<<16)|1U;assert(max31855_read(&value)==MAX31855_OPEN_CIRCUIT);
 assert(!g_hw_debug.tc_valid && isnan(g_hw_debug.tc_display_c));
 frame=120U<<18;assert(max31855_read(&value)==MAX31855_OK);assert(g_hw_debug.tc_display_c==30);
 g_hw_tuning.display_filter_alpha=1;frame=128U<<18;
 assert(max31855_read(&value)==MAX31855_OK && g_hw_debug.tc_display_c==32);
 spi_ok=false;assert(max31855_read(&value)==MAX31855_SPI_ERROR && g_hw_debug.tc_errors==2);
 puts("Slow encoder, tuning, wrap, bounce, temperature display filter and faults: PASS");return 0;
}
