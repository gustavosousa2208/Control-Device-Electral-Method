/* Host integration test against the actual firmware FatFs and Unicode code.
 * Only opens a caller-created regular test image, never a physical drive. */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifdef _WIN32
#include <io.h>
#define image_seek _fseeki64
#define image_resize(file, size) _chsize_s(_fileno(file), (size))
#else
#include <unistd.h>
#define image_seek fseeko
#define image_resize(file, size) ftruncate(fileno(file), (off_t)(size))
#endif
#include "ff.h"
#include "diskio.h"
#if _FS_EXFAT != 1 || _USE_LFN != 1
#error "Test must use the firmware exFAT/static-LFN configuration"
#endif
static FILE *image;
static const DWORD sectors=1048576;
static int io_sector(void *buf,DWORD sector,UINT count,int write){
 if((uint64_t)sector+count>sectors||image_seek(image,(int64_t)sector*512,SEEK_SET))return 0;
 return (write?fwrite(buf,512,count,image):fread(buf,512,count,image))==count;
}
DSTATUS disk_initialize(BYTE drive){return drive?STA_NOINIT:0;}
DSTATUS disk_status(BYTE drive){return drive?STA_NOINIT:0;}
DRESULT disk_read(BYTE drive,BYTE *p,DWORD s,UINT n){return drive?RES_PARERR:(io_sector(p,s,n,0)?RES_OK:RES_ERROR);}
DRESULT disk_write(BYTE drive,const BYTE *p,DWORD s,UINT n){return drive?RES_PARERR:(io_sector((void*)p,s,n,1)?RES_OK:RES_ERROR);}
DRESULT disk_ioctl(BYTE drive,BYTE command,void *p){
 if(drive)return RES_PARERR;
 switch(command){case CTRL_SYNC:return fflush(image)?RES_ERROR:RES_OK;
 case GET_SECTOR_COUNT:*(DWORD*)p=sectors;return RES_OK;
 case GET_SECTOR_SIZE:*(WORD*)p=512;return RES_OK;
 case GET_BLOCK_SIZE:*(DWORD*)p=1;return RES_OK;default:return RES_PARERR;}
}
DWORD get_fattime(void){return ((2026UL-1980)<<25)|(10UL<<21)|(17UL<<16);}
static void check_image(BYTE format,BYTE expected){
 static BYTE work[32768],data[17000],back[17000];FATFS fs;FIL f;UINT count;FILINFO info;
 for(unsigned i=0;i<sizeof(data);i++)data[i]=(BYTE)(i*31U+7U);
 assert(f_mkfs("0:",format,4096,work,sizeof(work))==FR_OK);
 assert(f_mount(&fs,"0:",1)==FR_OK&&fs.fs_type==expected);
 assert(f_open(&f,"0:/BCH001.TXT",FA_WRITE|FA_CREATE_NEW)==FR_OK);
 assert(f_write(&f,data,sizeof(data),&count)==FR_OK&&count==sizeof(data));
 assert(f_sync(&f)==FR_OK&&f_close(&f)==FR_OK);
 assert(f_mkdir("0:/MEDI\207OES")==FR_OK);
 const char *name="0:/MEDI\207OES/ENSAIO TERMICO COM NOME LONGO.TXT";
 assert(f_open(&f,name,FA_WRITE|FA_CREATE_NEW)==FR_OK);
 assert(f_write(&f,data,sizeof(data),&count)==FR_OK&&count==sizeof(data));
 assert(f_close(&f)==FR_OK);
 assert(f_rename(name,"0:/MEDI\207OES/ENSAIO RENOMEADO.TXT")==FR_OK);
 assert(f_mount(NULL,"0:",0)==FR_OK&&f_mount(&fs,"0:",1)==FR_OK);
 assert(f_open(&f,"0:/MEDI\207OES/ENSAIO RENOMEADO.TXT",FA_READ)==FR_OK);
 assert(f_read(&f,back,sizeof(back),&count)==FR_OK&&count==sizeof(back)&&!memcmp(data,back,sizeof(data)));
 assert(f_close(&f)==FR_OK);
 assert(f_stat("0:/BCH001.TXT",&info)==FR_OK&&info.fsize==sizeof(data));
 assert(f_open(&f,"0:/BCH001.TXT",FA_WRITE|FA_CREATE_NEW)==FR_EXIST);
 assert(f_unlink("0:/BCH001.TXT")==FR_OK);
 assert(f_mount(NULL,"0:",0)==FR_OK);
 printf("%s: MBR mount, cross-cluster IO, sync, remount, CP850 long name, rename and no-overwrite PASS\n",expected==FS_EXFAT?"exFAT":"FAT32");
}
int main(int argc,char **argv){
 assert(argc==2&&!strstr(argv[1],"\\\\.\\")&&!strstr(argv[1],"PhysicalDrive"));
 image=fopen(argv[1],"w+b");assert(image);
 assert(!image_resize(image,(uint64_t)sectors*512));
 check_image(FM_FAT32,FS_FAT32);check_image(FM_EXFAT,FS_EXFAT);
 fclose(image);return 0;
}
