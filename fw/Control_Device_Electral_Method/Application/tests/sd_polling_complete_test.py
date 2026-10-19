from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
source=(root/'Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_sd.c').read_text()
start=source.index('HAL_StatusTypeDef HAL_SD_ReadBlocks(')
brace=source.index('{',start);depth=1;end=brace+1
while depth:
 depth+=(source[end]=='{')-(source[end]=='}');end+=1
function=source[start:end]
mock=r'''
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
typedef enum {HAL_OK,HAL_ERROR,HAL_TIMEOUT} HAL_StatusTypeDef;
typedef struct {uint32_t DCTRL,flags;} SDIO_TypeDef;
typedef struct {uint32_t DataTimeOut,DataLength,DataBlockSize,TransferDir,TransferMode,DPSM;} SDIO_DataInitTypeDef;
typedef struct {SDIO_TypeDef *Instance;uint32_t ErrorCode,State,Context;struct {uint32_t LogBlockNbr,CardType;} SdCard;} SD_HandleTypeDef;
#define HAL_SD_STATE_READY 1
#define HAL_SD_STATE_BUSY 2
#define HAL_SD_ERROR_NONE 0
#define HAL_SD_ERROR_PARAM 1
#define HAL_SD_ERROR_ADDR_OUT_OF_RANGE 2
#define HAL_SD_ERROR_TIMEOUT 4
#define HAL_SD_ERROR_DATA_TIMEOUT 8
#define HAL_SD_ERROR_DATA_CRC_FAIL 16
#define HAL_SD_ERROR_RX_OVERRUN 32
#define HAL_SD_ERROR_BUSY 64
#define SDIO_FLAG_RXOVERR 1
#define SDIO_FLAG_DCRCFAIL 2
#define SDIO_FLAG_DTIMEOUT 4
#define SDIO_FLAG_DATAEND 8
#define SDIO_FLAG_RXFIFOHF 16
#define SDIO_FLAG_RXDAVL 32
#define SDIO_STATIC_DATA_FLAGS 15
#define SDIO_STATIC_FLAGS 15
#define SDMMC_DATATIMEOUT 1000
#define SDIO_DATABLOCK_SIZE_512B 0
#define SDIO_TRANSFER_DIR_TO_SDIO 0
#define SDIO_TRANSFER_MODE_BLOCK 0
#define SDIO_DPSM_ENABLE 1
#define SD_CONTEXT_READ_MULTIPLE_BLOCK 2
#define SD_CONTEXT_READ_SINGLE_BLOCK 1
#define SD_CONTEXT_NONE 0
#define CARD_SDHC_SDXC 1
#define CARD_SECURED 2
#define BLOCKSIZE 512
#define __HAL_SD_CLEAR_FLAG(h,f) ((h)->Instance->flags&=~(f))
#define __HAL_SD_GET_FLAG(h,f) (((h)->Instance->flags&(f))!=0)
static uint32_t ticks,words,mode,goal;
uint32_t HAL_GetTick(void){return ticks++;}
uint32_t SDIO_ReadFIFO(SDIO_TypeDef *p){uint32_t v=0x11220000U+words++;if(words==goal)p->flags=SDIO_FLAG_DATAEND;return v;}
uint32_t SDIO_ConfigData(SDIO_TypeDef*p,SDIO_DataInitTypeDef*c){(void)p;goal=c->DataLength/4;return 0;}
uint32_t SDMMC_CmdReadSingleBlock(SDIO_TypeDef*p,uint32_t a){
 (void)a;assert(!(p->flags&SDIO_FLAG_DATAEND));
 p->flags=mode==1?SDIO_FLAG_DATAEND:mode==2?SDIO_FLAG_DCRCFAIL:mode==3?(SDIO_FLAG_DATAEND|SDIO_FLAG_RXDAVL):SDIO_FLAG_RXFIFOHF;return 0;
}
uint32_t SDMMC_CmdReadMultiBlock(SDIO_TypeDef*p,uint32_t a){return SDMMC_CmdReadSingleBlock(p,a);}
uint32_t SDMMC_CmdStopTransfer(SDIO_TypeDef*p){(void)p;return 0;}
'''
tests=r'''
int main(void){
 SDIO_TypeDef p={0};SD_HandleTypeDef h={.Instance=&p,.State=HAL_SD_STATE_READY,.SdCard={10000,CARD_SDHC_SDXC}};
 uint8_t data[1024];
 p.flags=SDIO_FLAG_DATAEND;mode=0;assert(HAL_SD_ReadBlocks(&h,data,0,1,1000)==HAL_OK&&words==128&&h.Context==0);
 words=ticks=0;p.flags=SDIO_FLAG_DATAEND;mode=1;memset(data,0xA5,sizeof(data));
 assert(HAL_SD_ReadBlocks(&h,data,0,1,1000)==HAL_ERROR&&h.ErrorCode==HAL_SD_ERROR_DATA_TIMEOUT&&h.Context==0);
 assert(data[0]==0xA5&&words==0);
 words=ticks=0;mode=2;assert(HAL_SD_ReadBlocks(&h,data,0,1,1000)==HAL_ERROR&&h.ErrorCode==HAL_SD_ERROR_DATA_CRC_FAIL);
 words=ticks=0;mode=3;assert(HAL_SD_ReadBlocks(&h,data,0,1,1000)==HAL_OK&&words==128);
 words=ticks=0;mode=0;assert(HAL_SD_ReadBlocks(&h,data,0,2,1000)==HAL_OK&&words==256);
 puts("Actual HAL SD polling: stale DATAEND, incomplete read rejection, CRC error, FIFO tail and multiblock PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='sd-polling-') as temp:
 path=Path(temp);(path/'test.c').write_text(mock+function+tests)
 subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror',str(path/'test.c'),'-o',str(path/'test.exe')],check=True)
 subprocess.run([str(path/'test.exe')],check=True)
