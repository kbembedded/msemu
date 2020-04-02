#ifndef __FLASHOPS_H__
#define __FLASHOPS_H__

#include <stdint.h>
#include <stdio.h>
#include "msemu.h"

#define DF_SN_OFFS	0x7FFC8

/* Struct to handle software data protection of the 28SF040
 * The dataflash starts out locked, so in order to get back to the locked
 * state, the whole array must be traversed. In msemu, a counter can be
 * used to watch the reads, once the unlock set is matched, then the device
 * will remain unlocked until successfully matching the lock set.
 */
extern const uint16_t df_unlock_lock_arr[];

void df_unlock(void);
void df_lock(void);

/**
 * Interpret commands intended for 28SF040 flash, aka Mailstation dataflash
 *
 * ms              - Struct of ms_hw used by main emulation
 * translated_addr - Address in range of dataflash, 0x00000:0x7FFFF
 * val             - Command or value to send to dataflash
 */
int df_parse_cmd(ms_ctx* ms, unsigned int translated_addr, uint8_t val);

/**
 * Generate and set a random serial number in dataflash
 *
 * See flashops.c for more detail about the serial number
 *
 * ms - Pointer to ms_ctx which already has dev_map set up and allocated
 */
void df_set_rnd_serial(ms_ctx *ms);

/**
 * Check if serial number in dataflash is valid
 *
 * See flashops.c for more detail about the serial number
 *
 * ms - Pointer to ms_ctx which already has dev_map set up and allocated
 */
int df_serial_valid(ms_ctx *ms);

/**
 * Reads a file into memory.
 *
 * buf       - buffer to load file contents into
 * file_path - path to file on disk
 * sz        - number of bytes to read
 *
 * Returns number of bytes read.
 */
int filetobuf(uint8_t *buf, const char *file_path, ssize_t sz);

/**
 * Writes a buffer to file.
 *
 * buf       - contents to be written
 * file_path - path to file on disk
 * sz        - number of bytes to write
 *
 * Returns number of bytes written.
 */
int buftofile(uint8_t *buf, const char *file_path, ssize_t sz);
#endif
