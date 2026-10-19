#include "sdlog.h"
#include "hw_debug.h"
#include "runtime_config.h"
#include "format.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sd_io.h"
volatile uint32_t g_sd_filesystem_type;
static bool mounted, open_run;
static char run_name[32];
bool sdlog_ready(void)
{
#if THERMAL_USE_FATFS
    if (mounted && !board_sd_is_ready()) { mounted = false; g_sd_filesystem_type = 0; }
#endif
    return mounted;
}
const char *sdlog_run_name(void) { return run_name; }
#if THERMAL_USE_FATFS
#include "ff.h"
#include "stm32f4xx_hal.h"
static FATFS fs;
static FIL run;
static bool write_exact(FIL *file, const char *text)
{
    UINT written = 0;
    size_t length = strlen(text);
    return length <= 512U && f_write(file,text,(UINT)length,&written) == FR_OK &&
        written == length && f_sync(file) == FR_OK;
}
static bool storage_error(void)
{
    mounted = false; g_sd_filesystem_type = 0;
    if (open_run) { (void)f_close(&run); open_run = false; }
    return false;
}
void sdlog_init(void)
{
    g_sd_filesystem_type = 0;
    open_run = false; run_name[0] = 0;
    g_hw_debug.sd_mount_result = -1;
    mounted = false;
    /* A cold card can identify successfully before its first sector reads are
     * reliable. Repeat the existing manual recovery sequence at most twice.
     * Mounting is read-only; never retry a failed file write here. This runs
     * only at startup or the explicit idle long-press, not in the job loop. */
    for (unsigned attempt = 0; attempt < 3U && board_sd_is_ready(); ++attempt) {
        FRESULT result = f_mount(&fs,THERMAL_SD_VOLUME,1);
        g_hw_debug.sd_mount_result = result;
        mounted = result == FR_OK;
        g_sd_filesystem_type = mounted ? fs.fs_type : 0U;
        if (mounted || attempt == 2U) break;
        (void)f_mount(NULL,THERMAL_SD_VOLUME,0);
        HAL_Delay(250U);
        if (!board_sd_prepare()) break;
    }
}
bool sdlog_open_run(bool calibration)
{
    if (!mounted || open_run) return false;
    for (unsigned index = 1; index <= 999; ++index) {
        (void)snprintf(run_name,sizeof(run_name),"%s/%s_%03u.CSV",THERMAL_SD_VOLUME,
                       calibration?"CAL":"TST",index);
        FRESULT result = f_open(&run,run_name,FA_WRITE|FA_CREATE_NEW);
        if (result == FR_EXIST) continue;
        if (result != FR_OK) return storage_error();
        open_run = true;
        const char *header = calibration ? "idx,t_alvo_c,t_rtd_c,vf_v\r\n" :
            "idx,t_alvo_c,t_rtd_c,vf_v,tj_c,t_s\r\n";
        if (!write_exact(&run,header)) return storage_error();
        return true;
    }
    run_name[0] = 0; return false; /* No free slot; never overwrite a run. */
}
bool sdlog_line(const char *line)
{
    if (!mounted || !open_run || !line || strlen(line) > 250U) return false;
    char buffer[256];
    int n = snprintf(buffer,sizeof(buffer),"%s\r\n",line);
    if (n < 0 || (size_t)n >= sizeof(buffer)) return false;
    if (!write_exact(&run,buffer)) return storage_error();
    return true;
}
bool sdlog_close(void)
{
    if (!open_run) return mounted;
    bool synced = f_sync(&run) == FR_OK;
    bool closed = f_close(&run) == FR_OK;
    open_run = false;
    if (!synced || !closed) { mounted = false; g_sd_filesystem_type = 0; }
    return synced && closed;
}
static bool valid_cal(const cal_params_t *c)
{
    return c && c->valid && c->n >= 2 && c->n <= TARGETS_MAX && isfinite(c->m) &&
        fabsf(c->m) > 0.000001f && isfinite(c->t0) && isfinite(c->r2) &&
        c->r2 >= 0 && c->r2 <= 1;
}
static bool parse_cal(char *text, cal_params_t *out)
{
    const char *header = "m,t0,r2,n\r\n";
    if (strncmp(text,header,strlen(header)) != 0) return false;
    char *p = text+strlen(header), *end;
    cal_params_t candidate = {0};
    candidate.m = strtof(p,&end); if (end == p || *end++ != ',') return false; p=end;
    candidate.t0 = strtof(p,&end); if (end == p || *end++ != ',') return false; p=end;
    candidate.r2 = strtof(p,&end); if (end == p || *end++ != ',') return false; p=end;
    unsigned long n = strtoul(p,&end,10);
    if (end == p || n > UINT16_MAX || strncmp(end,"\r\n",2) != 0) return false;
    const tc_cal_params_t *tc=tc_calibration_get();
    char *crc_start;
    if (strncmp(end+2,"#tc=",4)==0) {
        p=end+6;float gain=strtof(p,&end);
        if(end==p || *end!=',')return false;
        p=end+1;float offset=strtof(p,&end);
        if(end==p || !isfinite(gain) || !isfinite(offset) ||
           fabsf(gain-tc->gain)>0.00001f || fabsf(offset-tc->offset)>0.0002f ||
           strncmp(end,"\r\n#crc=",7)!=0)return false;
        crc_start=end+2;
    } else {
        if(fabsf(tc->gain-1)>0.00001f || fabsf(tc->offset)>0.0002f || strncmp(end,"\r\n#crc=",7)!=0)return false;
        crc_start=end+2;
    }
    unsigned long stored_crc = strtoul(crc_start+5,&end,16);
    if (strcmp(end,"\r\n") != 0 || stored_crc > UINT32_MAX ||
        thermal_crc32((const uint8_t*)text,(size_t)(crc_start-text)) != stored_crc)
        return false;
    candidate.n = (uint16_t)n; candidate.valid = true;
    if (!valid_cal(&candidate)) return false;
    *out = candidate; return true;
}
static bool load_file(const char *name, cal_params_t *out)
{
    FIL file; char text[160]; UINT bytes = 0;
    if (f_open(&file,name,FA_READ) != FR_OK) return false;
    FRESULT result = f_read(&file,text,sizeof(text)-1,&bytes);
    bool closed = f_close(&file) == FR_OK;
    if (result != FR_OK || !closed || bytes >= sizeof(text)-1 ||
        memchr(text,0,bytes) != NULL) return false;
    text[bytes] = 0; return parse_cal(text,out);
}
bool sdlog_load_calibration(cal_params_t *out)
{
    if (!out) return false;
    *out = (cal_params_t){0};
    if (!mounted) return false;
    return load_file(THERMAL_SD_VOLUME "/CALIB.TXT",out) ||
           load_file(THERMAL_SD_VOLUME "/CALIB.BAK",out);
}
bool sdlog_save_calibration(const cal_params_t *c)
{
    if (!mounted || open_run || !valid_cal(c)) return false;
    char m[24], t0[24], r2[24], text[160],tc_gain[24],tc_offset[24];
    const tc_cal_params_t *tc=tc_calibration_get();
    if(!format_decimal(tc_gain,sizeof(tc_gain),tc->gain,6) || !format_decimal(tc_offset,sizeof(tc_offset),tc->offset,4))return false;
    if (!format_decimal(m,sizeof(m),c->m,3) || !format_decimal(t0,sizeof(t0),c->t0,4) ||
        !format_decimal(r2,sizeof(r2),c->r2,6)) return false;
    int n = snprintf(text,sizeof(text),"m,t0,r2,n\r\n%s,%s,%s,%u\r\n#tc=%s,%s\r\n",m,t0,r2,c->n,tc_gain,tc_offset);
    if (n < 0 || (size_t)n+18 >= sizeof(text)) return false;
    uint32_t crc = thermal_crc32((const uint8_t *)text,(size_t)n);
    (void)snprintf(text+n,sizeof(text)-(size_t)n,"#crc=%08lX\r\n",(unsigned long)crc);
    FIL temporary;
    if (f_open(&temporary,THERMAL_SD_VOLUME "/CALIB.TMP",FA_WRITE|FA_CREATE_ALWAYS) != FR_OK)
        return storage_error();
    bool written = write_exact(&temporary,text);
    bool closed = f_close(&temporary) == FR_OK;
    if (!written || !closed) return storage_error();
    cal_params_t verified;
    if (!load_file(THERMAL_SD_VOLUME "/CALIB.TMP",&verified)) return storage_error();
    /* Preserve previous valid calibration as backup before replacing it.
     * FAT rename is not an atomic power-loss transaction; test on actual card. */
    FRESULT result;
    if (load_file(THERMAL_SD_VOLUME "/CALIB.TXT",&verified)) {
        result = f_unlink(THERMAL_SD_VOLUME "/CALIB.BAK");
        if (result != FR_OK && result != FR_NO_FILE) return storage_error();
        result = f_rename(THERMAL_SD_VOLUME "/CALIB.TXT",THERMAL_SD_VOLUME "/CALIB.BAK");
        if (result != FR_OK) return storage_error();
    } else {
        /* If primary is corrupt/absent, retain existing valid backup. */
        result = f_unlink(THERMAL_SD_VOLUME "/CALIB.TXT");
        if (result != FR_OK && result != FR_NO_FILE) return storage_error();
    }
    if (f_rename(THERMAL_SD_VOLUME "/CALIB.TMP",THERMAL_SD_VOLUME "/CALIB.TXT") != FR_OK)
        return storage_error();
    return true;
}

/* Independent, versioned calibration of MAX31855 + probe. */
static bool tc_load_file(const char *name, tc_cal_params_t *out)
{
 FIL file; char text[160]; UINT bytes=0;
 if (f_open(&file,name,FA_READ)!=FR_OK) return false;
 FRESULT read=f_read(&file,text,sizeof(text)-1,&bytes);
 bool closed=f_close(&file)==FR_OK;
 if (read!=FR_OK || !closed || bytes>=sizeof(text)-1 || memchr(text,0,bytes)) return false;
 text[bytes]=0;
 const char *header="tc_v1\r\n";
 if (strncmp(text,header,strlen(header))!=0) return false;
 char *cursor=text+strlen(header),*end;
 float values[4];
 for(unsigned i=0;i<4;i++) {
  values[i]=strtof(cursor,&end);
  if (end==cursor || *end!=',') return false;
  cursor=end+1;
 }
 unsigned long points=strtoul(cursor,&end,10);
 if (end==cursor || points>4 || strncmp(end,"\r\n#crc=",7)!=0) return false;
 char *crc_start=end+2;
 unsigned long crc=strtoul(end+7,&end,16);
 if (strcmp(end,"\r\n")!=0 || crc>UINT32_MAX || crc!=thermal_crc32((const uint8_t*)text,(size_t)(crc_start-text))) return false;
 tc_cal_params_t p={values[0],values[1],values[2],values[3],(unsigned)points};
 if (!tc_calibration_valid(&p)) return false;
 *out=p;return true;
}
bool sdlog_load_tc_calibration(tc_cal_params_t *out)
{
 return out && mounted && (tc_load_file(THERMAL_SD_VOLUME "/TCAL.TXT",out) || tc_load_file(THERMAL_SD_VOLUME "/TCAL.BAK",out));
}
bool sdlog_save_tc_calibration(const tc_cal_params_t *p)
{
 if (!mounted || open_run || !tc_calibration_valid(p)) return false;
 char gain[24],offset[24],alpha[24],r2[24],text[160];
 if (!format_decimal(gain,sizeof(gain),p->gain,6) || !format_decimal(offset,sizeof(offset),p->offset,4) ||
     !format_decimal(alpha,sizeof(alpha),p->alpha,3) || !format_decimal(r2,sizeof(r2),p->r2,6)) return false;
 int n=snprintf(text,sizeof(text),"tc_v1\r\n%s,%s,%s,%s,%u\r\n",gain,offset,alpha,r2,p->points);
 if (n<0 || (size_t)n+18>=sizeof(text)) return false;
 uint32_t crc=thermal_crc32((const uint8_t*)text,(size_t)n);
 (void)snprintf(text+n,sizeof(text)-(size_t)n,"#crc=%08lX\r\n",(unsigned long)crc);
 FIL file;
 if (f_open(&file,THERMAL_SD_VOLUME "/TCAL.TMP",FA_WRITE|FA_CREATE_ALWAYS)!=FR_OK) return storage_error();
 bool written=write_exact(&file,text),closed=f_close(&file)==FR_OK;
 tc_cal_params_t verified;
 if (!written || !closed || !tc_load_file(THERMAL_SD_VOLUME "/TCAL.TMP",&verified)) return storage_error();
 FRESULT result;
 if (tc_load_file(THERMAL_SD_VOLUME "/TCAL.TXT",&verified)) {
  result=f_unlink(THERMAL_SD_VOLUME "/TCAL.BAK");
  if (result!=FR_OK && result!=FR_NO_FILE) return storage_error();
  if (f_rename(THERMAL_SD_VOLUME "/TCAL.TXT",THERMAL_SD_VOLUME "/TCAL.BAK")!=FR_OK) return storage_error();
 } else {
  result=f_unlink(THERMAL_SD_VOLUME "/TCAL.TXT");
  if (result!=FR_OK && result!=FR_NO_FILE) return storage_error();
 }
 if (f_rename(THERMAL_SD_VOLUME "/TCAL.TMP",THERMAL_SD_VOLUME "/TCAL.TXT")!=FR_OK) return storage_error();
 return true;
}
bool sdlog_reset_tc_calibration(void)
{
    const tc_cal_params_t defaults={1,0,THERMAL_DISPLAY_FILTER_ALPHA,1,0};
    /* First commit replaces the active profile; second rotates defaults into
     * the backup too, so fallback cannot revive the old 3/4-point fit. */
    return sdlog_save_tc_calibration(&defaults) && sdlog_save_tc_calibration(&defaults);
}
#else
bool sdlog_save_tc_calibration(const tc_cal_params_t *p) { (void)p;return false; }
bool sdlog_reset_tc_calibration(void) { return false; }
bool sdlog_load_tc_calibration(tc_cal_params_t *p) { (void)p;return false; }
/* No fabricated successful storage when CubeMX FatFs is not integrated. */
void sdlog_init(void) { mounted = open_run = false; run_name[0] = 0; }
bool sdlog_open_run(bool calibration) { (void)calibration; return false; }
bool sdlog_line(const char *line) { (void)line; return false; }
bool sdlog_close(void) { return false; }
bool sdlog_save_calibration(const cal_params_t *cal) { (void)cal; return false; }
bool sdlog_load_calibration(cal_params_t *cal)
{
    if (cal) *cal = (cal_params_t){0};
    return false;
}
#endif
