#ifndef __ZPARLIB_H__
#define __ZPARLIB_H__

/* TODO: Document this better since it all changed */

#include <zmq.h>

/* TODO: make a more sane way to establish default url:port */
#define PRIMARY		0
#define SECONDARY	1
#define PRIPUBSOCK	"8888"
#define SECPUBSOCK	"9999"

#define ZPAR_DR		0
#define ZPAR_SR		1
#define ZPAR_CR		2

/* Register bits */
#define ZPAR_SR_ERROR	(1 << 3)
#define ZPAR_SR_SEL_IN	(1 << 4)
#define ZPAR_SR_PAP_OUT	(1 << 5)
#define ZPAR_SR_ACK	(1 << 6)
#define ZPAR_SR_BUSY	(1 << 7)
#define ZPAR_CR_STROBE	(1 << 0)
#define ZPAR_CR_AUTOF	(1 << 1)
#define ZPAR_CR_INIT	(1 << 2)
#define ZPAR_CR_SEL	(1 << 3)


/* Data to send, or to be received. */
struct pardat {
	uint8_t zpar_dat;
	uint8_t zpar_status;
	uint8_t zpar_ctrl;
	uint8_t refresh;
};

/* Support for different cables. The LapLink cable needs bit-swapping in order
 * to correctly connect the two endpoints. This affords very minimal changes
 * to applications that could use a real world cable vs purely emulated.
 *
 * Cable swizzling is disigned to only happen on the SECONDARY side.
 */
/* The primary should _ALWAYS_ use PRI so no cable encode/decode happens on
 * the MailStation primary end.
 */
#define ZPAR_CABLE_PRI		0

/* The secondary side should use one of the below cables to do bit translation
 * upon send or receive.
 */
#define ZPAR_CABLE_STRAIGHT	0
#define ZPAR_CABLE_LAPLINK	1

#define ZPAR_WAIT		0
#define ZPAR_DONTWAIT		1

/* These are assumed to be localhost most of the time */
struct conn_opts {
	char *pripubsock;
	char *secpubsock;
	char *priurl;
	char *securl;
	int conn_role;
	int conn_cable;
};

/* Functions needed for general operation */
void *zpar_init(const struct conn_opts *opts);
void zpar_deinit(void *zmq_handle);

/* Functions for the application maintaining their own TX and RX buffers */
int zpar_read(void *zmq_handle, struct pardat **pd, int flags);
int zpar_write(void *zmq_handle, struct pardat *pd, int flags);

/* Functions for the application using the zpar maintained buffers */
int zpar_ior(void *zmq_handle, uint8_t reg, uint8_t *val, int flags);
int zpar_iow(void *zmq_handle, uint8_t reg, uint8_t val, int flags);
/* Same as zpar_iow, but set/clear register bits */
int zpar_iow_set(void *zmq_handle, uint8_t reg, uint8_t bits, int flags);
int zpar_iow_clr(void *zmq_handle, uint8_t reg, uint8_t bits, int flags);

#endif // __ZPARLIB_H__


