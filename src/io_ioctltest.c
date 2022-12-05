#include <stdint.h>
#include <stdio.h>

#include "io.h"
#include "msemu.h"

int ioctl_test_write(void *dat, void *val)
{
	printf("Writing 0xaa to val\n");
	*(uint8_t *)val = 0xaa;
	return MS_OK;
}

//int ms_ioctl_register(ms_ctx *ms, int num, uint8_t (*func)(void *, int, uint8_t), void *dat)

int ioctl_test_init(ms_ctx *ms)
{
	ms_ioctl_register(ms, 1, ioctl_test_write, ms);
}

int ioctl_test_deinit(ms_ctx *ms)
{
	ms_ioctl_deregister(ms, 1);
}

//int ms_ioctl_deregister(ms_ctx *ms, int num)
