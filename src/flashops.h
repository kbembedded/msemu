#ifndef __FLASHOPS_H__
#define __FLASHOPS_H__

#include <stdint.h>
#include <stdio.h>
#include "msemu.h"

/**
 * Initialize buffer, open file, and copy contents to buffer
 *
 * **buf    - Pointer to pointer to buffer to use for flash
 * *options - Pointer to ms_opts struct w/ file path to open/copy contents from
 */
int df_init(uint8_t **df_buf, ms_opts *options);
int cf_init(uint8_t **cf_buf, ms_opts *options);

/**
 * Write buffer contents back to file (optionally not write back)
 *
 * **buf    - Pointer to point to buffer to use for flash
 * *options - Pointer to ms_opts struct w/ file path and save to disk opts
 */
int df_deinit(uint8_t **df_buf, ms_opts *options);
int cf_deinit(uint8_t **cf_buf, ms_opts *options);

/**
 * Interpret commands intended for 28SF040 flash, aka Mailstation dataflash
 *
 * *df_buf       - Pointer to buffer for dataflash
 * absolute_addr - Address in range of dataflash, 0x00000:0x7FFFF
 * val           - Command or value to send to dataflash
 */
int df_write(uint8_t *df_buf, unsigned int absolute_addr, uint8_t val);

/**
 * Return a byte from the Mailstation dataflash buffer. This does not
 * require any special command structure unlike the write path.
 *
 * *buf          - Pointer to buffer for flash
 * absolute_addr - Address in range of flash:
 *                 Dataflash 0x00000:0x7FFFF
 *                 Codeflash 0x00000:0xFFFFF
 */
uint8_t df_read(uint8_t *df_buf, unsigned int absolute_addr);
uint8_t cf_read(uint8_t *cf_buf, unsigned int absolute_addr);


/* Unimplemented at this time */
int cf_write(uint8_t *cf_buf, unsigned int absolute_addr, uint8_t val);

#endif // __FLASHOPS_H__
