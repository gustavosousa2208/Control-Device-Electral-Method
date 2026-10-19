"""Execute the actual polling-write function with a modeled SDIO peripheral.

No physical card access. Run independently of the full application suite:
    python3 Application/tests/sd_write_test.py
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_sd.c').read_text()
start = source.index('HAL_StatusTypeDef HAL_SD_WriteBlocks(')
function = source[start:source.index('\n/**', start)]
bsp = (root / 'FATFS/Target/bsp_driver_sd.c').read_text()
start = bsp.index('__weak uint8_t BSP_SD_WriteBlocks(')
bsp_function = bsp[start:bsp.index('\n}', start) + 2].replace('__weak ', '')
diskio = (root / 'FATFS/Target/sd_diskio.c').read_text()
start = diskio.index('DRESULT SD_ioctl(BYTE lun, BYTE cmd, void *buff)\n{')
ioctl_function = diskio[start:diskio.index('\n}', start) + 2]
fixture = r'''
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "sd_io.h"
typedef enum {HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT} HAL_StatusTypeDef;
enum {HAL_SD_ERROR_NONE=0, HAL_SD_ERROR_PARAM=1, HAL_SD_ERROR_ADDR_OUT_OF_RANGE=2,
 HAL_SD_ERROR_TIMEOUT=4, HAL_SD_ERROR_DATA_TIMEOUT=8, HAL_SD_ERROR_DATA_CRC_FAIL=16,
 HAL_SD_ERROR_TX_UNDERRUN=32, HAL_SD_ERROR_BUSY=64};
enum {SDIO_FLAG_TXUNDERR=1, SDIO_FLAG_DCRCFAIL=2, SDIO_FLAG_DTIMEOUT=4,
 SDIO_FLAG_DATAEND=8, SDIO_FLAG_TXFIFOHE=16};
enum {HAL_SD_STATE_READY=1, HAL_SD_STATE_BUSY, CARD_SDHC_SDXC, CARD_SECURED,
 SD_CONTEXT_NONE, SD_CONTEXT_WRITE_MULTIPLE_BLOCK, SD_CONTEXT_WRITE_SINGLE_BLOCK};
#define BLOCKSIZE 512U
#define SDMMC_DATATIMEOUT 0xFFFFFFFFU
#define SDIO_DATABLOCK_SIZE_512B 9U
#define SDIO_TRANSFER_DIR_TO_CARD 0U
#define SDIO_TRANSFER_MODE_BLOCK 0U
#define SDIO_DPSM_ENABLE 1U
#define SDIO_STATIC_FLAGS 0xFFFFFFFFU
#define SDIO_STATIC_DATA_FLAGS 0xFFFFFFFFU
typedef struct {uint32_t DCTRL, CLKCR, CMD, RESP1, STA, DCOUNT;} SDIO_TypeDef;
typedef struct {SDIO_TypeDef *Instance; uint32_t ErrorCode, State, Context;
 struct {uint32_t CardType, LogBlockNbr;} SdCard;} SD_HandleTypeDef;
typedef struct {uint32_t DataTimeOut, DataLength, DataBlockSize, TransferDir,
 TransferMode, DPSM;} SDIO_DataInitTypeDef;
static SDIO_TypeDef peripheral;
static uint32_t tick, flags, words, expected_words, cmd_error, commands, stops;
static uint32_t cmd_address, injected_error;
static int stall, early_end, command_seen;
static uint8_t transmitted[1024];
uint32_t HAL_GetTick(void) {return tick++;}
static uint32_t get_flag(uint32_t mask) {
 if (peripheral.DCTRL && command_seen) {
  if (injected_error) flags |= injected_error;
  else if (early_end || words == expected_words) flags |= SDIO_FLAG_DATAEND;
  else if (!stall) flags |= SDIO_FLAG_TXFIFOHE;
 }
 return flags & mask;
}
#define __HAL_SD_GET_FLAG(h, mask) get_flag(mask)
#define __HAL_SD_CLEAR_FLAG(h, mask) (flags &= ~(mask))
static uint32_t command(uint32_t addr) {
 /* The transmit path must stay idle throughout command/response exchange. */
 assert(peripheral.DCTRL == 0);
 command_seen=1; commands++; cmd_address=addr; return cmd_error;
}
uint32_t SDMMC_CmdWriteMultiBlock(SDIO_TypeDef *p,uint32_t addr) {
 assert(p==&peripheral);return command(addr);
}
uint32_t SDMMC_CmdWriteSingleBlock(SDIO_TypeDef *p,uint32_t addr) {
 assert(p==&peripheral);return command(addr);
}
uint32_t SDMMC_CmdStopTransfer(SDIO_TypeDef *p) {
 assert(p==&peripheral);stops++;return 0;
}
uint32_t SDIO_ConfigData(SDIO_TypeDef *p, SDIO_DataInitTypeDef *c) {
 assert(p==&peripheral && command_seen && !cmd_error);
 expected_words=c->DataLength/4; p->DCTRL=c->DPSM; return 0;
}
uint32_t SDIO_WriteFIFO(SDIO_TypeDef *p,uint32_t *data) {
 assert(p==&peripheral && words<expected_words && words<256);
 memcpy(transmitted+words*4,data,4); words++; flags &= ~SDIO_FLAG_TXFIFOHE;
 return 0;
}
FUNCTION_UNDER_TEST
static SD_HandleTypeDef reset(void) {
 peripheral.DCTRL=0;tick=flags=words=expected_words=cmd_error=commands=stops=0;
 injected_error=0;stall=early_end=command_seen=0;
 memset(transmitted,0,sizeof transmitted);
 SD_HandleTypeDef h={.Instance=&peripheral,.State=HAL_SD_STATE_READY,
 .SdCard={.CardType=CARD_SDHC_SDXC,.LogBlockNbr=1000}};return h;
}
static void test_polling(void) {
 uint8_t data[1025];for(unsigned i=0;i<sizeof data;i++)data[i]=(uint8_t)i;
 SD_HandleTypeDef h;
 for(unsigned blocks=1;blocks<=2;blocks++) {
  h=reset();assert(HAL_SD_WriteBlocks(&h,data+1,7,blocks,1000)==HAL_OK);
  assert(commands==1 && cmd_address==7 && words==128*blocks && stops==blocks-1);
  assert(memcmp(transmitted,data+1,512*blocks)==0);
  assert(h.State==HAL_SD_STATE_READY && h.Context==SD_CONTEXT_NONE);
 }
 h=reset();h.SdCard.CardType=0;
 assert(HAL_SD_WriteBlocks(&h,data,7,1,1000)==HAL_OK && cmd_address==7*512);
 h=reset();cmd_error=HAL_SD_ERROR_DATA_CRC_FAIL;
 assert(HAL_SD_WriteBlocks(&h,data,7,1,1000)==HAL_ERROR);
 assert(!peripheral.DCTRL && !words && h.ErrorCode==cmd_error);
 h=reset();stall=1;
 assert(HAL_SD_WriteBlocks(&h,data,7,1,10)==HAL_TIMEOUT);
 assert(h.ErrorCode==HAL_SD_ERROR_TIMEOUT && !peripheral.DCTRL);
 h=reset();early_end=1;
 assert(HAL_SD_WriteBlocks(&h,data,7,1,1000)==HAL_ERROR);
 assert(h.ErrorCode==HAL_SD_ERROR_DATA_TIMEOUT);
 const uint32_t errors[]={SDIO_FLAG_DCRCFAIL,SDIO_FLAG_TXUNDERR,SDIO_FLAG_DTIMEOUT};
 const uint32_t expected[]={HAL_SD_ERROR_DATA_CRC_FAIL,HAL_SD_ERROR_TX_UNDERRUN,
 HAL_SD_ERROR_DATA_TIMEOUT};
 for(unsigned i=0;i<3;i++) {
  h=reset();injected_error=errors[i];
  assert(HAL_SD_WriteBlocks(&h,data,7,1,1000)==HAL_ERROR);
  assert(h.ErrorCode==expected[i] && commands==1);
 }
 h=reset();assert(HAL_SD_WriteBlocks(&h,NULL,7,1,1000)==HAL_ERROR && !commands);
 h=reset();assert(HAL_SD_WriteBlocks(&h,data,7,0,1000)==HAL_ERROR && !commands);
 h=reset();assert(HAL_SD_WriteBlocks(&h,data,999,2,1000)==HAL_ERROR && !commands);
 h=reset();assert(HAL_SD_WriteBlocks(&h,data,UINT32_MAX,2,1000)==HAL_ERROR && !commands);
 h=reset();h.State=HAL_SD_STATE_BUSY;
 assert(HAL_SD_WriteBlocks(&h,data,7,1,1000)==HAL_ERROR && !commands);
 puts("SD polling write: ordering, payload, addressing, errors, timeout: PASS");
}
typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef enum {RES_OK,RES_ERROR,RES_NOTRDY,RES_PARERR} DRESULT;
typedef struct {uint32_t LogBlockNbr,LogBlockSize;} BSP_SD_CardInfo;
#define MSD_OK 0U
#define MSD_ERROR 1U
#define STA_NOINIT 1U
#define CTRL_SYNC 0U
#define GET_SECTOR_COUNT 1U
#define GET_SECTOR_SIZE 2U
#define GET_BLOCK_SIZE 3U
#define SD_DEFAULT_BLOCK_SIZE 512U
#define SD_TIMEOUT 10U
static uint32_t Stat, busy_polls;
SD_HandleTypeDef hsd;
volatile sd_io_debug_t g_sd_io_debug;
uint8_t BSP_SD_GetCardState(void) {
 hsd.ErrorCode=0; /* Model later CMD13 erasing the transfer error. */
 if (busy_polls) {busy_polls--;return MSD_ERROR;}return MSD_OK;
}
void BSP_SD_GetCardInfo(BSP_SD_CardInfo *c) {c->LogBlockNbr=1000;c->LogBlockSize=512;}
BSP_UNDER_TEST
IOCTL_UNDER_TEST
int main(void) {
 test_polling();
 uint8_t data[512]={0};
 hsd=reset();injected_error=SDIO_FLAG_TXUNDERR;
 assert(BSP_SD_WriteBlocks((uint32_t *)data,7,1,1000)==MSD_ERROR);
 assert(g_sd_io_debug.write_attempts==1 && g_sd_io_debug.write_failures==1);
 assert(g_sd_io_debug.write_hal_status==HAL_ERROR);
 assert(g_sd_io_debug.write_hal_error==HAL_SD_ERROR_TX_UNDERRUN);
 BSP_SD_GetCardState();
 assert(hsd.ErrorCode==0 && g_sd_io_debug.last_failed_write_error==HAL_SD_ERROR_TX_UNDERRUN);
 hsd=reset();assert(BSP_SD_WriteBlocks((uint32_t *)data,8,1,1000)==MSD_OK);
 assert(g_sd_io_debug.write_hal_error==0 && g_sd_io_debug.write_sector==8);
 assert(g_sd_io_debug.last_failed_write_error==HAL_SD_ERROR_TX_UNDERRUN);
 busy_polls=3;assert(SD_ioctl(0,CTRL_SYNC,NULL)==RES_OK && !busy_polls);
 busy_polls=100;assert(SD_ioctl(0,CTRL_SYNC,NULL)==RES_ERROR);
 tick=UINT32_MAX-3;busy_polls=100;
 assert(SD_ioctl(0,CTRL_SYNC,NULL)==RES_ERROR);
 Stat=STA_NOINIT;assert(SD_ioctl(0,CTRL_SYNC,NULL)==RES_NOTRDY);
 puts("SD write diagnostics and bounded CTRL_SYNC (including tick wrap): PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='electral-sd-write-') as temp:
    path = Path(temp)
    (path / 'test.c').write_text(fixture.replace('FUNCTION_UNDER_TEST', function)
                               .replace('BSP_UNDER_TEST', bsp_function)
                               .replace('IOCTL_UNDER_TEST', ioctl_function))
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-Wno-unused-parameter', '-I' + str(root / 'Application/board'),
                    '-fsanitize=address,undefined', str(path / 'test.c'),
                    '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
