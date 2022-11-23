#ifndef __VPARLIB_H__
#define __VPARLIB_H__

/* Based on FS-UAE VPAR implemention however this uses 0MQ under the hood.
 * More details on VPAR can be found here: https://fs-uae.net/docs/vpar
 */

/* The following is verbatim from FS-UAE */
/* Virtual parallel port protocol aka "vpar"
 *
 * Always sends/receives 2 bytes: one control byte, one data byte-
 *
 * Send to UAE the following control byte:
 *     0x01 0 = BUSY line value
 *     0x02 1 = POUT line value
 *     0x04 2 = SELECT line value
 *     0x08 3 = trigger ACK (and IRQ if enabled)
 *
 *     0x10 4 = set following data byte
 *     0x20 5 = set line value as given in bits 0..2
 *     0x40 6 = set line bits given in bits 0..2
 *     0x80 7 = clr line bits given in bits 0..2
 *
 * Receive from UAE the following control byte:
 *     0x01 0 = BUSY line set
 *     0x02 1 = POUT line set
 *     0x04 2 = SELECT line set
 *     0x08 3 = STROBE was triggered
 *     0x10 4 = is an reply to a read request (otherwise emu change)
 *     0x20 5 = n/a
 *     0x40 6 = emulator is starting (first msg of session)
 *     0x80 7 = emulator is shutting down (last msg of session)
 *
 * Note: sending a 00,00 pair returns the current state pair.
 */

#include <zmq.h>

#define HOST		0
#define DEVICE		1
#define HOSTPUBSOCK	"8888"
#define DEVPUBSOCK	"9999"

#define VPAR_DR		0
#define VPAR_SR		1
#define VPAR_CR		2

/* Register bits */
#define VPAR_SR_ERROR	(1 << 3)
#define VPAR_SR_SEL_IN	(1 << 4)
#define VPAR_SR_PAP_OUT	(1 << 5)
#define VPAR_SR_ACK	(1 << 6)
#define VPAR_SR_BUSY	(1 << 7)
#define VPAR_CR_STROBE	(1 << 0)
#define VPAR_CR_AUTOF	(1 << 1)
#define VPAR_CR_INIT	(1 << 2)
#define VPAR_CR_SEL	(1 << 3)


/* Data to send, or to be received. */
struct pardat {
	uint8_t vpar_dat;
	uint8_t vpar_status
	uint8_t vpar_ctrl;
	uint8_t refresh;
};

#define VPAR_MODE_NONE	0
#define VPAR_MODE_MS	0
#define VPAR_MODE_DEV	1

/* These are assumed to be localhost most of the time */
struct conn_opts {
	char *hostpubsock;
	char *devpubsock;
	char *hosturl;
	char *devurl;
	int conn_role;
	int conn_mode;
};

void *vpar_init(int mode, struct zmq_opts *opts);
int vpar_read(void *zmq_handle, struct pardat *read);
int vpar_read_nonblocking(
int vpar_read_blocking(
int vpar_read_and_update(
int vpar_write(void *zmq_handle, struct pardat *write);
int vpar_write_nonblocking(
int vpar_write_blocking
void vapr_deinit(void *zmq_handle);

#endif // __VPARLIB_H__


