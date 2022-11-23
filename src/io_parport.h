#ifndef __IO_PARPORT_H__
#define __IO_PARPORT_H__

#include "msemu.h"
#include "io_parport.h"

void *pp_init(void);
int pp_deinit(void *handle);
int pp_read(void *handle, size_t offs);
int pp_write(void *handle, size_t offs, uint8_t val);

#endif // __IO_PARPORT_H__
