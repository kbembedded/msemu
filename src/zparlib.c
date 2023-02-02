#include <assert.h>
#include <error.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zmq.h>

#include "zparlib.h"

/* The reads and writes here don't care about any kind of contention, it is
 * purely what is read from and written to the parallel port bus.
 * It is up to individual applications to decide what to do with the read or
 * written data, as well as (presumably) maintain their own direction info.
 *
 * That is, if the MailStation changes its port state, it triggers a send,
 * once the device recv's it, it is the signal that was read across the
 * lines. The device needs to decide if a logical 0 or 1 is written by
 * the MailStation, or by the device itself based on its own internal states.
 */

struct zmq_handle {
	void *zmq_ctx;
	void *zmq_pub;
	void *zmq_sub;
	int conn_cable;
	int conn_role;
	struct pardat *rx;
	struct pardat *tx;
};

static void *zmq_conn_init(const struct conn_opts *opts)
{
	struct zmq_handle *zmqh = NULL;
	const int zmqtrue = 1;
	char *pripubstr = NULL;
	char *hostsubstr = NULL;
	char *secpubstr = NULL;
	char *devsubstr = NULL;

	zmqh = malloc(sizeof(struct zmq_handle));
	/* XXX: Error checking */
	memset(zmqh, '\0', sizeof(struct zmq_handle));

	zmqh->rx = malloc(sizeof(struct pardat));
	/* XXX: Error checking */
	memset(zmqh->rx, '\0', sizeof(struct pardat));

	zmqh->tx = malloc(sizeof(struct pardat));
	/* XXX: Error checking */
	memset(zmqh->tx, '\0', sizeof(struct pardat));

	/* These will be at most, 5 bytes for port, 6 bytes for protocol,
	 * 257 bytes for hostname/IP, and null termination, 269 bytes */
	pripubstr = malloc(269);
	hostsubstr = malloc(269);
	secpubstr = malloc(269);
	devsubstr = malloc(269);
	strncpy(pripubstr, "tcp://*:", 9);
	strncat(pripubstr, opts->pripubsock, 5);
	strncpy(hostsubstr, "tcp://localhost:", 17);
	strncat(hostsubstr, opts->secpubsock, 5);
	strncpy(secpubstr, "tcp://*:", 9);
	strncat(secpubstr, opts->secpubsock, 5);
	strncpy(devsubstr, "tcp://localhost:", 17);
	strncat(devsubstr, opts->pripubsock, 5);

	zmqh->conn_cable = opts->conn_cable;
	zmqh->conn_role = opts->conn_role;

	zmqh->zmq_ctx = zmq_ctx_new();
	zmqh->zmq_pub = zmq_socket(zmqh->zmq_ctx, ZMQ_PUB);
	zmqh->zmq_sub = zmq_socket(zmqh->zmq_ctx, ZMQ_SUB);

	/* Only keep the most recent message */
	zmq_setsockopt(zmqh->zmq_sub, ZMQ_CONFLATE, &zmqtrue, sizeof(zmqtrue));
	zmq_setsockopt(zmqh->zmq_pub, ZMQ_CONFLATE, &zmqtrue, sizeof(zmqtrue));
	/* XXX: Error checking with teardown on failure */

	/* Subscribe to all events */
	zmq_setsockopt(zmqh->zmq_sub, ZMQ_SUBSCRIBE, "", 0);
	/* XXX: Error checking with teardown on failure */

	/* XXX: Make this way more flexible */
	if (zmqh->conn_role == PRIMARY) {
		/* Bind the host publish socket
		 * For now, 8888 is the host (MS) publish socket
		 * 9999 is the device (whatever is plugged in to the MS) publish
		 */
		zmq_bind(zmqh->zmq_pub, pripubstr);

		/* Connect to device publish socket, our subscribe */
		zmq_connect(zmqh->zmq_sub, hostsubstr);
	} else {
		/* Bind the device publish socket */
		zmq_bind(zmqh->zmq_pub, secpubstr);

		/* Connect to host publish socket, our subscribe */
		zmq_connect(zmqh->zmq_sub, devsubstr);
	}

	return zmqh;
}

static void zmq_conn_deinit(void *handle)
{
	struct zmq_handle *zmqh = handle;
	zmq_close(zmqh->zmq_pub);
	zmq_close(zmqh->zmq_sub);
	zmq_ctx_term(zmqh->zmq_ctx);
	free(zmqh->rx);
	free(zmqh->tx);
	free(zmqh);
	zmqh = NULL;
}

void *zpar_init(const struct conn_opts *opts)
{
	return zmq_conn_init(opts);
}

static void bit_swap_u8(uint8_t *reg1, int pos1, uint8_t *reg2, int pos2)
{
	/* XXX: Naive swap at the moment. Could probably be optimized */
	uint8_t reg1_save, reg2_save;

	reg1_save = !!(*reg1 & (1 << pos1));
	reg2_save = !!(*reg2 & (1 << pos2));
	*reg1 &= ~(1 << pos1);
	*reg2 &= ~(1 << pos2);
	*reg1 |= (reg2_save << pos1);
	*reg2 |= (reg1_save << pos2);
}

static inline void bit_nc_u8(uint8_t *reg1, int pos1)
{
	*reg1 &= ~(1 << pos1);
}

static inline void bit_inv_u8(uint8_t *reg1, int pos1)
{
	*reg1 ^= (1 << pos1);
}

static void laplink_enc_dec(struct pardat *pd)
{
	bit_swap_u8(&pd->zpar_dat, 0, &pd->zpar_status, 3);
	bit_swap_u8(&pd->zpar_dat, 1, &pd->zpar_status, 4);
	bit_swap_u8(&pd->zpar_dat, 2, &pd->zpar_status, 5);
	bit_swap_u8(&pd->zpar_dat, 3, &pd->zpar_status, 6);
	bit_swap_u8(&pd->zpar_dat, 4, &pd->zpar_status, 7);
	bit_nc_u8(&pd->zpar_dat, 5);
	bit_nc_u8(&pd->zpar_dat, 6);
	bit_nc_u8(&pd->zpar_dat, 7);
}

/* Called on zpar_write */
static void cable_encode(struct zmq_handle *zmqh, struct pardat *pd)
{
	switch (zmqh->conn_cable) {
	case ZPAR_CABLE_LAPLINK:
		/* A laplink cable ends up connecting some dat to ctrl/status
		 * signals.
		 * With a standard laplink cable, the following signals are switched:
		 * dat0 <-> error
		 * dat1 <-> sel
		 * dat2 <-> paper end
		 * dat3 <-> ack
		 * dat4 <-> busy
		 *
		 * The following signals are NC:
		 * dat5
		 * dat6
		 * dat7
		 * 
		 * All other signals are straight through
		 */
		laplink_enc_dec(pd);
		break;
	}
}

/* call on zpar_read ONLY when data is available since it modifes
 * the partdat struct. */
static int cable_decode(struct zmq_handle *zmqh, struct pardat *pd)
{
	switch (zmqh->conn_cable) {
	case ZPAR_CABLE_LAPLINK:
		/* A laplink cable ends up connecting some dat to ctrl/status
		 * signals.
		 * With a standard laplink cable, the following signals are switched:
		 * dat0 <-> error
		 * dat1 <-> sel
		 * dat2 <-> paper end
		 * dat3 <-> ack
		 * dat4 <-> busy
		 *
		 * The following signals are NC:
		 * dat5
		 * dat6
		 * dat7
		 * 
		 * All other signals are straight through
		 */
		laplink_enc_dec(pd);

		/* IBM modes of operation have a few pins inverted in hardware */
		//bit_inv_u8(&pd->zpar_status, 3);
		//bit_inv_u8(&pd->zpar_status, 6);
		//bit_inv_u8(&pd->zpar_status, 7);
		//bit_inv_u8(&pd->zpar_ctrl, 3);
		//bit_inv_u8(&pd->zpar_ctrl, 1);
		//bit_inv_u8(&pd->zpar_ctrl, 0);

		break;
	}
}

int zpar_read(void *zmq_handle, struct pardat **pd, int flags)
{
	struct zmq_handle *zmqh = zmq_handle;
	int rc;

	/* Read bytes in to allocated struct. If no data ready, struct is
	 * untouched. In either case, update the provided pointer */
	rc = zmq_recv(zmqh->zmq_sub, zmqh->rx, sizeof(struct pardat),
			(flags == ZPAR_DONTWAIT) ? ZMQ_DONTWAIT : 0);

	if (rc != -1) {
		/* Only decode a received packet once */
		cable_decode(zmqh, zmqh->rx);
	}

	if (zmqh->rx->refresh) {
		/* Always do this as a DONTWAIT. The reason being that we don't
		 * want a surprise secondary wait state on a read.
		 */
		rc = zpar_write(zmq_handle, zmqh->tx, ZPAR_DONTWAIT);
		zmqh->rx->refresh = 0;
	}

	if (pd != NULL)
		*pd = zmqh->rx;

	return rc;
}

int zpar_ior(void *handle, uint8_t reg, uint8_t *val, int flags)
{
	struct pardat *pd;
	int rc;

	assert(reg < sizeof(struct pardat));

	rc = zpar_read(handle, &pd, flags);

	switch (reg) {
	case ZPAR_DR:
		*val = pd->zpar_dat;
		break;
	case ZPAR_SR:
		*val = pd->zpar_status;
		break;
	case ZPAR_CR:
		*val = pd->zpar_ctrl;
	}

	return rc;
}

int zpar_iow(void *handle, uint8_t reg, uint8_t val, int flags)
{
	struct zmq_handle *zmqh = handle;

	assert(reg < sizeof(struct pardat));

	switch (reg) {
	case ZPAR_DR:
		zmqh->tx->zpar_dat = val;
		break;
	case ZPAR_SR:
		zmqh->tx->zpar_status = val;
		break;
	case ZPAR_CR:
		zmqh->tx->zpar_ctrl = val;
		break;
	}

	return zpar_write(handle, NULL, flags);
}

int zpar_iow_set(void *handle, uint8_t reg, uint8_t bits, int flags)
{
	struct zmq_handle *zmqh = handle;

	assert(reg < sizeof(struct pardat));

	switch (reg) {
	case ZPAR_DR:
		zmqh->tx->zpar_dat |= bits;
		break;
	case ZPAR_SR:
		zmqh->tx->zpar_status |= bits;
		break;
	case ZPAR_CR:
		zmqh->tx->zpar_ctrl |= bits;
		break;
	}

	return zpar_write(handle, NULL, flags);
}

int zpar_iow_clr(void *handle, uint8_t reg, uint8_t bits, int flags)
{
	struct zmq_handle *zmqh = handle;

	assert(reg < sizeof(struct pardat));

	switch (reg) {
	case ZPAR_DR:
		zmqh->tx->zpar_dat &= ~(bits);
		break;
	case ZPAR_SR:
		zmqh->tx->zpar_status &= ~(bits);
		break;
	case ZPAR_CR:
		zmqh->tx->zpar_ctrl &= ~(bits);
		break;
	}

	return zpar_write(handle, NULL, flags);
}

/* If pd is NULL, write the current buffer */
int zpar_write(void *handle, struct pardat *pd, int flags)
{
	struct zmq_handle *zmqh = handle;
	struct pardat pd_tmp;
	int rc;

	/* If pd is NULL, write the current zmqh->tx buffer.
	 * If pd is provided, copy it to zmqh->tx buffer first.
	 *
	 * Note that the buffer is not stored cable-encoded, this means that
	 * when pd is null, the zmqh->tx buffer must first be copied,
	 * cable-encoded, and then sent.
	 *
	 * It is necessary to store the not cable_encoded buffer since the iow
	 * functions can be used to modify the buffer and then send it out.
	 */
	if (pd == NULL) {
		memcpy(&pd_tmp, zmqh->tx, sizeof(struct pardat));
		cable_encode(zmqh, &pd_tmp);
		rc = zmq_send(zmqh->zmq_pub, &pd_tmp, sizeof(struct pardat),
			(flags == ZPAR_DONTWAIT) ? ZMQ_DONTWAIT : 0);
	} else {
		memcpy(zmqh->tx, pd, sizeof(struct pardat));
		cable_encode(zmqh, pd);
		rc = zmq_send(zmqh->zmq_pub, pd, sizeof(struct pardat),
			(flags == ZPAR_DONTWAIT) ? ZMQ_DONTWAIT : 0);
	}

	return rc;
}

void zpar_deinit(void *handle)
{
	zmq_conn_deinit(handle);
}

int zpar_dump_state(void *handle)
{
	struct zmq_handle *zmqh = handle;
	//struct pardat pd_tmp;

	/* The RX buffer is already cable_decoded, however, the TX buffer
	 * is stored not cable encoded and must be switched here*/
	//memcpy(&pd_tmp, zmqh->tx, sizeof(struct pardat));
	//cable_encode(zmqh, &pd_tmp);
	printf("TX buf:\nDR: 0x%02X; SR: 0x%02X, CR: 0x%02X\n",
		zmqh->tx->zpar_dat, zmqh->tx->zpar_status, zmqh->tx->zpar_ctrl);
	printf("RX buf:\nDR: 0x%02X; SR: 0x%02X, CR: 0x%02X\n",
		zmqh->rx->zpar_dat, zmqh->rx->zpar_status, zmqh->rx->zpar_ctrl);

	return 0;
}
