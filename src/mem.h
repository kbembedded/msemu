#ifndef __FLASHOPS_H__
#define __FLASHOPS_H__

#include <stdint.h>
#include <stdio.h>
#include "msemu.h"

/**
 * Initialize buffer, open file, and copy contents to buffer
 *
 * *ms		- Pointer to ms_ctx struct
 * *options	- Pointer to ms_opts struct w/ file path to open/copy contents from
 */
int df_init(ms_ctx *ms);
int cf_init(ms_ctx *ms);
int ram_init(ms_ctx *ms);

/**
 * Populate buffers from files if applicable
 */
int df_populate(ms_ctx *ms, ms_opts *options);
int cf_populate(ms_ctx *ms, ms_opts *options);
int ram_populate(ms_ctx *ms, ms_opts *options);

/**
 * Write buffer contents back to file (optionally not write back)
 *
 * *ms		- Pointer to ms_ctx struct
 * *options	- Pointer to ms_opts struct w/ file path and save to disk opts
 *
 * ram_deinit does not write buffer back to disk
 */
int df_deinit(ms_ctx *ms, ms_opts *options);
int cf_deinit(ms_ctx *ms, ms_opts *options);
int ram_deinit(ms_ctx *ms);

/**
 * Interpret commands intended for 28SF040 flash, aka Mailstation dataflash
 *
 * *ms		 - Pointer to ms_ctx struct
 * absolute_addr - Address in range of dataflash, 0x00000:0x7FFFF
 * val           - Command or value to send to dataflash
 */
int df_write(ms_ctx *ms, unsigned int absolute_addr, uint8_t val);
int ram_write(ms_ctx *ms, unsigned int absolute_addr, uint8_t val);

/**
 * Return a byte from the Mailstation dataflash buffer. This does not
 * require any special command structure unlike the write path.
 *
 * *ms		 - Pointer to ms_ctx struct
 * absolute_addr - Address in range of flash:
 *                 Dataflash 0x00000:0x7FFFF
 *                 Codeflash 0x00000:0xFFFFF
 *                 RAM       0x00000:0x20000
 */
uint8_t df_read(ms_ctx *ms, unsigned int absolute_addr);
uint8_t cf_read(ms_ctx *ms, unsigned int absolute_addr);
uint8_t ram_read(ms_ctx *ms, unsigned int absolute_addr);


/* Unimplemented at this time */
int cf_write(ms_ctx *ms, unsigned int absolute_addr, uint8_t val);

#endif // __FLASHOPS_H__
