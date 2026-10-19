#include <assert.h>
#include <string.h>
#include "oled.h"
#include "peripheral_io.h"
uint32_t board_millis(void) { return 0; }
static unsigned driver=1, count, pages, bytes;
static bool bus=true, prepare=true;
static uint8_t commands[64];
bool board_oled_prepare(void){return prepare;}
uint8_t board_oled_controller(void){return (uint8_t)driver;}
uint8_t board_oled_column_offset(void){return driver==2?2:0;}
bool board_oled_write(const uint8_t *p,size_t n){
 if(!bus)return false;
 if(n==2){assert(p[0]==0);assert(count<64);commands[count++]=p[1];}
 else if(n==4){assert(p[0]==0 && p[1]==0xB0+pages && p[2]==board_oled_column_offset() && p[3]==0x10);pages++;}
 else {assert(n==17 && p[0]==0x40);bytes+=16;}
 return true;
}
int main(void){
 for(driver=1;driver<=2;driver++){
  count=pages=bytes=0;oled_init();assert(oled_ready());
  if(driver==1){assert(count==28 && commands[1]==0x20 && commands[2]==2 && commands[25]==0x8D && commands[26]==0x14);}
  else {assert(count==22 && commands[8]==0xAD && commands[9]==0x8B);}
  while(oled_busy())oled_service();
  assert(pages==8 && bytes==1024);oled_service();assert(commands[count-1]==0xAF);
  assert(oled_present());bus=false;oled_service();assert(!oled_ready() && !oled_busy());bus=true;
 }
 count=0;prepare=false;oled_init();assert(!oled_ready() && count==0);
 prepare=true;bus=false;oled_init();assert(!oled_ready());bus=true;
 driver=99;oled_init();assert(!oled_ready());return 0;
}
