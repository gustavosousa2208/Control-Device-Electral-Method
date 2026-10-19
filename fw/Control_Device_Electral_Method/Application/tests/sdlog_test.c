#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "ff.h"
#include "sdlog.h"
static struct { char name[40], content[2048]; size_t size; } files[16];
static bool card=true, short_write;
static unsigned mount_calls, mount_failures, prepares, delays, opens;
static bool prepare_ok=true;
bool board_sd_is_ready(void) { return card; }
bool board_sd_prepare(void) { prepares++;return prepare_ok; }
void HAL_Delay(uint32_t ms) { assert(ms==250U);delays++; }
static int find(const char *name)
{
    for(int i=0;i<16;i++) if(strcmp(files[i].name,name)==0)return i;
    return -1;
}
FRESULT f_mount(FATFS *fs,const char *path,BYTE opt)
{
    (void)path;(void)opt;
    if (!fs) return FR_OK;
    mount_calls++;
    if (mount_failures) { mount_failures--;return FR_DISK_ERR; }
    return card?FR_OK:FR_DISK_ERR;
}
FRESULT f_open(FIL *file,const char *name,BYTE mode)
{
    opens++;
    if(!card)return FR_DISK_ERR;
    int i=find(name);
    if(i>=0 && (mode&FA_CREATE_NEW))return FR_EXIST;
    if(i<0 && !(mode&(FA_CREATE_NEW|FA_CREATE_ALWAYS)))return FR_NO_FILE;
    if(i<0) for(int j=0;j<16;j++) if(!files[j].name[0]){i=j;break;}
    if(i<0)return FR_DISK_ERR;
    if(mode&(FA_CREATE_NEW|FA_CREATE_ALWAYS)) {
        (void)snprintf(files[i].name,sizeof(files[i].name),"%s",name); files[i].size=0;
    }
    file->index=i;file->pos=0;return FR_OK;
}
FRESULT f_write(FIL *file,const void *data,UINT count,UINT *written)
{
    *written=0;if(!card)return FR_DISK_ERR;
    if(short_write && count)count--;
    int i=file->index; assert(file->pos+count<=sizeof(files[i].content));
    memcpy(files[i].content+file->pos,data,count);file->pos+=count;files[i].size=file->pos;
    *written=count;return FR_OK;
}
FRESULT f_read(FIL *file,void *data,UINT count,UINT *read)
{
    if(!card)return FR_DISK_ERR;
    int i=file->index;size_t available=files[i].size-file->pos;
    if(count>available)count=(UINT)available;
    memcpy(data,files[i].content+file->pos,count);file->pos+=count;*read=count;return FR_OK;
}
FRESULT f_sync(FIL *file) { (void)file;return card?FR_OK:FR_DISK_ERR; }
FRESULT f_close(FIL *file) { (void)file;return card?FR_OK:FR_DISK_ERR; }
FRESULT f_unlink(const char *path)
{
    if(!card)return FR_DISK_ERR;
    int i=find(path);if(i<0)return FR_NO_FILE;
    files[i].name[0]=0;files[i].size=0;return FR_OK;
}
FRESULT f_rename(const char *from,const char *to)
{
    if(!card)return FR_DISK_ERR;
    int i=find(from);if(i<0)return FR_NO_FILE;if(find(to)>=0)return FR_EXIST;
    (void)snprintf(files[i].name,sizeof(files[i].name),"%s",to);return FR_OK;
}
int main(void)
{
    mount_failures=1;sdlog_init();assert(sdlog_ready());
    assert(mount_calls==2 && prepares==1 && delays==1 && opens==0);
    mount_calls=prepares=delays=0;mount_failures=10;
    sdlog_init();assert(!sdlog_ready());
    assert(mount_calls==3 && prepares==2 && delays==2 && opens==0);
    mount_calls=prepares=delays=0;mount_failures=1;prepare_ok=false;
    sdlog_init();assert(!sdlog_ready() && mount_calls==1 && prepares==1);
    prepare_ok=true;mount_failures=0;
    sdlog_init();assert(sdlog_ready());
    assert(sdlog_open_run(true)); assert(strcmp(sdlog_run_name(),"0:/CAL_001.CSV")==0);
    assert(sdlog_line("0,25.00,25.00,0.60000"));assert(sdlog_close());
    assert(sdlog_open_run(true));assert(strcmp(sdlog_run_name(),"0:/CAL_002.CSV")==0);
    assert(sdlog_close());
    cal_params_t cal={-357,279,1,3,true}, loaded;
    assert(sdlog_save_calibration(&cal));assert(sdlog_load_calibration(&loaded));
    assert(loaded.valid && loaded.m==cal.m && loaded.t0==cal.t0 && loaded.n==3);
    cal.t0=280;assert(sdlog_save_calibration(&cal));
    int current=find("0:/CALIB.TXT");assert(current>=0);files[current].content[14]^=1;
    assert(sdlog_load_calibration(&loaded) && loaded.t0==279); /* CRC fallback backup. */
    tc_cal_params_t tc={1.02f,-0.5f,0.25f,1,3},tc_loaded;
    assert(sdlog_save_tc_calibration(&tc) && sdlog_load_tc_calibration(&tc_loaded));
    assert(fabsf(tc_loaded.gain-1.02f)<0.00001f && tc_loaded.offset==-0.5f);
    assert(tc_calibration_set(&tc));assert(!sdlog_load_calibration(&loaded));
    assert(sdlog_save_calibration(&cal) && sdlog_load_calibration(&loaded));
    tc.alpha=0.5f;assert(tc_calibration_set(&tc));assert(sdlog_load_calibration(&loaded));
    tc.offset=1;assert(sdlog_save_tc_calibration(&tc));
    int tc_file=find("0:/TCAL.TXT");assert(tc_file>=0);files[tc_file].content[10]^=1;
    assert(sdlog_load_tc_calibration(&tc_loaded) && tc_loaded.offset==-0.5f);
    assert(sdlog_open_run(false));assert(!sdlog_save_tc_calibration(&tc));assert(sdlog_close());
    assert(sdlog_reset_tc_calibration());
    assert(sdlog_load_tc_calibration(&tc_loaded) && tc_loaded.gain==1 && tc_loaded.offset==0 && tc_loaded.points==0);
    tc_file=find("0:/TCAL.TXT");files[tc_file].content[10]^=1;
    assert(sdlog_load_tc_calibration(&tc_loaded) && tc_loaded.gain==1 && tc_loaded.offset==0 && tc_loaded.points==0);
    tc.gain=-1;assert(!sdlog_save_tc_calibration(&tc));
    short_write=true;assert(sdlog_open_run(false)==false && !sdlog_ready());
    short_write=false;sdlog_init();assert(sdlog_open_run(false));
    card=false;assert(!sdlog_line("test") && !sdlog_ready());
    sdlog_init();assert(!sdlog_ready());
    puts("FatFs logging, file numbering, CRC/backup and removal: PASS");return 0;
}
