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

#include "vparlib.h"


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

struct test {
	void *zmq_ctx;
	void *zmq_pub;
	void *zmq_sub;
	int conn_mode;
	int conn_role;
	struct pardat *rx;
	struct pardat *tx;
};

static void *zmq_conn_init(struct conn_opts *opts)
{
	/* XXX: How to handle cable mode here? Who is responsible for that detail? e.g. wiring for tribbles or byte modes of MailStation */
	struct test *zmq_handle = NULL;
	const int zmqtrue = 1;

	zmq_handle = malloc(sizeof(struct test));
	/* XXX: Error checking */
	memset(zmq_handle, '\0', sizeof(struct test));

	zmq_handle->rx = malloc(sizeof(struct pardat));
	/* XXX: Error checking */
	memset(zmq_handle->rx, '\0', sizeof(struct pardat));

	zmq_handle->tx = malloc(sizeof(struct pardat));
	/* XXX: Error checking */
	memset(zmq_handle->tx, '\0', sizeof(struct pardat));

	zmq_handle->conn_mode = opts->mode;

	zmq_handle->zmq_ctx = zmq_ctx_new();
	zmq_handle->zmq_pub = zmq_socket(zmq_handle->zmq_ctx, ZMQ_PUB);
	zmq_handle->zmq_sub = zmq_socket(zmq_handle->zmq_ctx, ZMQ_SUB);

	/* Only keep the most recent message */
	printf("%d\n", zmq_setsockopt(zmq_handle->zmq_sub, ZMQ_CONFLATE, &zmqtrue, sizeof(zmqtrue)));
	printf("%d\n", zmq_setsockopt(zmq_handle->zmq_pub, ZMQ_CONFLATE, &zmqtrue, sizeof(zmqtrue)));
	/* XXX: Error checking with teardown on failure */

	/* Subscribe to all events */
	zmq_setsockopt(zmq_handle->zmq_sub, ZMQ_SUBSCRIBE, "", 0);
	/* XXX: Error checking with teardown on failure */

	/* XXX: Make this way more flexible */
	if (zmq_handle->conn_mode == HOST) {
		/* Bind the host publish socket
		 * For now, 8888 is the host (MS) publish socket
		 * 9999 is the device (whatever is plugged in to the MS) publish
		 */
		printf("%d\n",zmq_bind(zmq_handle->zmq_pub, "tcp://*:"opts->hostpubsock));

		/* Connect to device publish socket, our subscribe */
		printf("%d\n",zmq_connect(zmq_handle->zmq_sub, "tcp://localhost:"opts->devpubsock));
	} else {
		/* Bind the device publish socket */
		printf("%d\n",zmq_bind(zmq_handle->zmq_pub, "tcp://*:"opts->devpubsock));

		/* Connect to host publish socket, our subscribe */
		printf("%d\n",zmq_connect(zmq_handle->zmq_sub, "tcp://localhost:"opts->devpubsock));
	}

	return zmq_handle;
}

static void zmq_conn_deinit(void *zmq_handle)
{
	struct test *zmqh = zmq_handle;
	zmq_close(zmqh->zmq_pub);
	zmq_close(zmqh->zmq_sub);
	zmq_ctx_term(zmqh->zmq_ctx);
	free(zmqh->rx);
	free(zmqh->tx);
	free(zmqh);
	zmq_handle = NULL;
}

void *vpar_init(struct conn_opts *opts)
{
	return zmq_conn_init(opts);
}

int vpar_read(void *zmq_handle, struct pardat **pd)
{
	struct test *zmqh = zmq_handle;
	int rc;

	/* XXX: Check for NULL vars */
	/* XXX: Should we read to a temporary buffer first? this way we
	 * could check the update request first and then respond to it?
	 * but what happens if our handle has fucky data? Would we need
	 * to go all the way back to the IO layer? I don't think there is
	 * any good way around that unless we maintain a send and receive
	 * buffer.. that might be the way to do it... */
	/* Read bytes in to allocated struct. If no data ready, struct is
	 * untouched. In either case, update the provided pointer */
	rc = zmq_recv(zmqh->zmq_sub, zmqh->rx, sizeof(struct pardat),
			ZMQ_DONTWAIT);

	/* XXX: Handle byte swizzling as necessary */
	if (zmq->rx.refresh) {
		/* XXX: How to handle recv working but send failing or vice
		 * versa? */
		rc = zmq_send(zmqh->zmq_pub, zmqh->tx, sizeof(struct pardat),
			ZMQ_DONTWAIT);
	}

	pd = zmqh->rx;

	return rc;
}

int vpar_write(void *zmq_handle, struct pardat *write)
{
	struct test *zmqh = zmq_handle;
	int rc;

	/* XXX: Check for NULLs */
	rc = zmq_send(zmqh->zmq_pub, write, sizeof(struct pardat), 0);
	return rc;
}

void vapr_deinit(void *zmq_handle)
{
	/* XXX: Check for NULL */
	zmq_conn_deinit(zmq_handle);
}

