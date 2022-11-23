#include <assert.h>
#include <errno.h>
#include <error.h>

#include "zparlib.h"

/*
 * Theory of operation here is that the pardat struct maintained in the ZMQ
 * handle is what is on the wire. Both sides need to maintain their own idea
 * of data direction.
 *
 * In traditional parallel ports, bus contention happens quite often and there
 * usually is series resistors on both ends. A read of any of the associated
 * IBM model parallel port registers puts the pin as an input and reads the
 * data on it, while a write sets it as an output and sets the data on the pin.
 * The series resistors (or current limiters) prevent a direct short between
 * power and the shared grounds.
 *
 * The MailStation parallel port is simply on GPIO pins. Thes have an associated
 * data and direction register.
 *
 * The zparlib (name pending since it conflicts with and differs from FS-UAE's
 * implementation) emulates the standard IBM model of registers. It is up to
 * the actual integration of zparlib to convert this to a format used by either
 * side of the wire.
 *
 * Or rather, the data sent by each end is what it thinks should be on the bus
 * and then it is up to the receiving side to decode what it wants to make of
 * the bus state.
 *
 * The io_parport layer of the emulation is designed to read or update a single
 * register at a time, and then push all line states out.
 *
 * The IO layer of the MailStation is responsible for taking the data on the
 * lines as received taking DDR in to consideration. The argument here is that
 * the tristate buffers are inside of the IC, while this layer is the physical
 * transport layer.
 *
 * Essentially, the msemu IO layer maintains its register states internally
 * and this layer just moves data around.
 *
 * XXX: Do I need to move more out of here and in to zparlib? e.g. how do we
 * handle emulation between two devices connected by NULL printer cable
 * (e.g. MailStation to PC) vs a straight-through cable (e.g. printer connected
 * to MailStation) since this would affect where the bits all get connected.
 */

const struct conn_opts opts = {
	.pripubsock = "8888",
	.secpubsock = "9999",
	.priurl = "localhost",
	.securl = "localhost",
	.conn_role = PRIMARY,
	.conn_cable = ZPAR_CABLE_PRI,
};


void *pp_init(void)
{
	/* XXX: set up OPTS to allow setting blocking/nonblocking reads/writes */
	void *handle = zpar_init(&opts);
	return handle;
}

/* Open socket
 * maybe need to fork?
 */

int pp_deinit(void *handle)
{
	zpar_deinit(handle);
}

// shut everything down

/* XXX: Change offs to be uint8_t reg? */
/* XXX: This is full of hacks at the moment */
int pp_read(void *handle, size_t offs)
{
	int rc;
	uint8_t val, reg;
	struct pardat *pd = NULL;

	/* XXX: Could error a little more gracefully */
	assert(offs < sizeof(struct pardat));

	rc = zpar_ior(handle, offs, &val, ZPAR_DONTWAIT);
	/* If we got an error, just print it on the screen, there isn't any way
	 * to gracefully handle any errors from ZPAR inside the MailStation.
	 */
	if (rc != -1 && rc != sizeof(struct pardat)) {
		printf("Incorrect number of bytes received over socket! Got %d expected %ld\n",
			rc, sizeof(struct pardat));
	}

	return val;
}

/* XXX: I think this needs to update EVERYTHING on every write just to nsure
 * a consistent bus state? */
/* XXX: I think I also painted myself in to a corner and need to re-think the
 * tx and rx buffers hidden in the handle. I either need to maintain a struct
 * at the io_parport level, or, rebuild the struct from the current MS regs each time its going to
 * be sent out */
int pp_write(void *handle, size_t offs, uint8_t val)
{
	struct pardat pd;

	/* XXX: Could error more gracefully */

	assert(offs < sizeof(struct pardat));

	return zpar_iow(handle, offs, val, ZPAR_DONTWAIT);
}


