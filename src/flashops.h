#ifndef __FLASHOPS_H__
#define __FLASHOPS_H__
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "msemu.h"

int8_t writeDataflash(MSHW* ms, unsigned int translated_addr, uint8_t val);
int flashtobuf(uint8_t *buf, const char *file_path, ssize_t sz);
int buftoflash(uint8_t *buf, const char *file_path, ssize_t sz);
#endif
