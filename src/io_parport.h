#ifndef __IO_PARPORT_H__
#define __IO_PARPORT_H__

#include "msemu.h"
#include "io_parport.h"

int pp_init(void *handle);
int pp_deinit(void *handle);
int pp_read(void *handle);
int pp_write(void *handle);

#endif // __IO_PARPORT_H__
