#ifndef __FLASHOPS_H__
#define __FLASHOPS_H__

#include <stdint.h>
#include <stdio.h>
#include "msemu.h"

int8_t writeDataflash(MSHW* ms, unsigned int translated_addr, uint8_t val);

/**
 * Reads a file into memory.
 *
 * buf       - buffer to load file contents into
 * file_path - path to file on disk
 * sz        - number of bytes to read
 *
 * Returns number of bytes read.
 */
int flashtobuf(uint8_t *buf, const char *file_path, ssize_t sz);

/**
 * Writes a buffer to file.
 *
 * buf       - contents to be written
 * file_path - path to file on disk
 * sz        - number of bytes to write
 *
 * Returns number of bytes written.
 */
int buftoflash(uint8_t *buf, const char *file_path, ssize_t sz);
#endif
