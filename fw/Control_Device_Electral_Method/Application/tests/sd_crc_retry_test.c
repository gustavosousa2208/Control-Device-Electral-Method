#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "sd_io.h"
#ifdef __weak
#undef __weak
#endif
#define __weak
#define MSD_OK 0
#define MSD_ERROR 1
#define HAL_OK 0
#define HAL_ERROR 1
#define HAL_SD_ERROR_DATA_CRC_FAIL 2U
#define HAL_SD_CARD_TRANSFER 4U
typedef int HAL_StatusTypeDef;
typedef struct {uint32_t CLKCR,CMD,RESP1;} registers;
static registers regs;
static struct {registers *Instance;uint32_t ErrorCode;} hsd={&regs,0};
volatile sd_io_debug_t g_sd_io_debug;
static unsigned calls,failures,delays;
static uint32_t fault=2,state=4;
static HAL_StatusTypeDef HAL_SD_ReadBlocks(void *handle,uint8_t *p,uint32_t sector,uint32_t n,uint32_t timeout) {
 (void)handle;(void)sector;(void)n;(void)timeout;calls++;
 if(calls<=failures){hsd.ErrorCode=fault;return HAL_ERROR;}
 hsd.ErrorCode=0;p[510]=0x55;p[511]=0xAA;return HAL_OK;
}
static uint32_t HAL_SD_GetCardState(void *h){(void)h;return state;}
static void HAL_Delay(uint32_t ms){assert(ms==2);delays++;}
#include "sd_crc_read_under_test.inc"
static void reset(unsigned n){calls=delays=0;failures=n;fault=2;state=4;memset((void*)&g_sd_io_debug,0,sizeof(g_sd_io_debug));}
int main(void){
 uint32_t data[256]={0};reset(1);
 assert(BSP_SD_ReadBlocks(data,0,1,1000)==0 && calls==2 && delays==1);
 assert(g_sd_io_debug.read_crc_failures==1 && g_sd_io_debug.read_hal_error==0 && g_sd_io_debug.last_failed_read_error==2);
 assert(g_sd_io_debug.first_read_signature==0x55AA);
 reset(5);assert(BSP_SD_ReadBlocks(data,0,1,1000)==1 && calls==3 && delays==2);
 reset(1);fault=4;assert(BSP_SD_ReadBlocks(data,0,1,1000)==1 && calls==1 && delays==0);
 reset(1);state=7;assert(BSP_SD_ReadBlocks(data,0,1,1000)==1 && calls==1);
 reset(1);assert(BSP_SD_ReadBlocks(data,0,2,1000)==1 && calls==1);
 return 0;
}
