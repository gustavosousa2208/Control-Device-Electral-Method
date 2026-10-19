#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "oled.h"
#include "peripheral_io.h"
static uint32_t now;
static unsigned attempts;
static bool device, writes_ok=true;
uint32_t board_millis(void){return now;}
bool board_oled_prepare(void){attempts++;return device;}
uint8_t board_oled_controller(void){return OLED_DRIVER_SSD1306;}
uint8_t board_oled_column_offset(void){return 0;}
bool board_oled_write(const uint8_t *p,size_t n){(void)p;(void)n;return writes_ok;}
int main(void){
 oled_init();assert(!oled_ready() && attempts==1);
 now=999;oled_recover_service();assert(attempts==1);
 now=1000;oled_recover_service();assert(attempts==2);
 device=true;now=2000;oled_recover_service();assert(oled_ready() && attempts==3);
 while(oled_busy()) { oled_service(); }
 oled_service();
 assert(oled_present());writes_ok=false;oled_service();assert(!oled_ready());
 writes_ok=true;now=3000;oled_recover_service();assert(oled_ready());
 device=false;oled_init();assert(!oled_ready());unsigned first=attempts;
 for(unsigned i=1;i<=5;i++){now+=1000;oled_recover_service();}
 assert(attempts==first+3);return 0;
}
