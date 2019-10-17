#ifndef __FLASHOPS_H__
#define __FLASHOPS_H__

#include <stdint.h>
#include <stdio.h>
#include "msemu.h"

/**
 * Interpret commands intended for 28SF040 flash, aka Mailstation dataflash
 *
 * ms              - Struct of ms_hw used by main emulation
 * translated_addr - Address in range of dataflash, 0x00000:0x7FFFF
 * val             - Command or value to send to dataflash
 */
int df_parse_cmd(ms_ctx* ms, unsigned int translated_addr, uint8_t val);

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
