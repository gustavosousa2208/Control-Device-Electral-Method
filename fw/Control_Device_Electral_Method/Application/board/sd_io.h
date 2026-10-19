#ifndef THERMAL_SD_IO_H
#define THERMAL_SD_IO_H
#include <stdbool.h>
#include <stdint.h>
/* Independent of g_hw_debug: retained results survive later status polling. */
typedef struct {
 uint32_t sector, signature, bytes_per_sector, sectors_per_cluster, reserved_sectors, fat_count;
 uint32_t total_sectors, fat_sectors, root_cluster, first_partition_lba, first_partition_type;
 uint8_t first_bytes[16], fat_type[8];
} sd_sector_debug_t;
typedef struct {
 uint32_t read_attempts, read_hal_status, read_hal_error, read_sector, read_blocks;
 uint32_t read_clkcr, read_cmd, read_response1;
 uint32_t last_card_state, card_status_hal_error;
 uint32_t card_type, card_logical_blocks;
 uint32_t first_read_signature;
 uint32_t read_retries,read_crc_failures,last_failed_read_error;
 uint32_t write_attempts, write_hal_status, write_hal_error, write_sector, write_blocks;
 uint32_t write_clkcr, write_cmd, write_response1, write_sta, write_dcount;
 uint32_t write_failures, last_failed_write_error;
 uint32_t sector_trace_count;
 sd_sector_debug_t sector_trace[4];
} sd_io_debug_t;
extern volatile sd_io_debug_t g_sd_io_debug;
/* Capture up to four distinct successful sector reads, never write the card. */
static inline uint32_t sd_le16(const uint8_t *p) {return (uint32_t)p[0]|((uint32_t)p[1]<<8);}
static inline uint32_t sd_le32(const uint8_t *p) {return sd_le16(p)|(sd_le16(p+2)<<16);}
static inline void sd_capture_sector(uint32_t sector,const uint8_t *p) {
 uint32_t n=g_sd_io_debug.sector_trace_count;
 for(uint32_t i=0;i<n;i++)if(g_sd_io_debug.sector_trace[i].sector==sector)return;
 if(n>=4U)return;
 volatile sd_sector_debug_t *d=&g_sd_io_debug.sector_trace[n];
 d->sector=sector;d->signature=sd_le16(p+510);d->bytes_per_sector=sd_le16(p+11);
 d->sectors_per_cluster=p[13];d->reserved_sectors=sd_le16(p+14);d->fat_count=p[16];
 d->total_sectors=sd_le16(p+19);if(!d->total_sectors)d->total_sectors=sd_le32(p+32);
 d->fat_sectors=sd_le16(p+22);if(!d->fat_sectors)d->fat_sectors=sd_le32(p+36);
 d->root_cluster=sd_le32(p+44);d->first_partition_lba=sd_le32(p+454);
 d->first_partition_type=p[450];
 for(unsigned i=0;i<16;i++)d->first_bytes[i]=p[i];
 for(unsigned i=0;i<8;i++)d->fat_type[i]=p[82+i];
 g_sd_io_debug.sector_trace_count=n+1U;
}
bool board_sd_prepare(void);
bool board_sd_is_ready(void);
#endif
