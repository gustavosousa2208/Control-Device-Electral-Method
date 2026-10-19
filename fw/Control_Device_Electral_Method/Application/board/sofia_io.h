#ifndef SOFIA_IO_H
#define SOFIA_IO_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {float raw;uint32_t switch_cycles,first_us,end_us;} sofia_capture_t;
bool sofia_io_capture(unsigned delay_us,sofia_capture_t *out);
uint32_t sofia_io_elapsed_us(uint32_t switch_cycles);
#endif
