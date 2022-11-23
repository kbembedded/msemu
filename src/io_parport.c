#include "errno.h"
#include "error.h"

#include "vparlib.h"

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
 * The vparlib (name pending since it conflicts with and differs from FS-UAE's
 * implementation) emulates the standard IBM model of registers. It is up to
 * the actual integration of vparlib to convert this to a format used by either
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
 * XXX: Do I need to move more out of here and in to vparlib? e.g. how do we
 * handle emulation between two devices connected by NULL printer cable
 * (e.g. MailStation to PC) vs a straight-through cable (e.g. printer connected
 * to MailStation) since this would affect where the bits all get connected.
 */

const struct conn_opts opts = {
	.hostpubsock = "8888",
	.devpubsock = "9999",
	.hosturl = "localhost",
	.devurl = "localhost",
	.conn_role = HOST,
	.conn_mode = VPAR_MODE_MS,
};


void *pp_init(void)
{
	/* XXX: set up OPTS to allow setting blocking/nonblocking reads/writes */
	void *handle = vpar_init(HOST, &opts);
	return handle;
}

/* Open socket
 * maybe need to fork?
 */

int pp_deinit(void *handle)
{
	vpar_deinit(handle);
}

// shut everything down

int pp_read(void *handle, offs_t offs)
{
	int rc;
	struct pardat *pd;

	/* XXX: Could error a little more gracefully */
	assert(offs < sizeof(struct pardat));

	/* XXX: Is the & necessary? */
	rc = vpar_read(handle, &pd);
	if (rc == -1 && errno == EAGAIN) {
		printf("No data received on read\n");
	} else if (rc != sizeof(struct pardat)) {
		printf("Incorrect number of bytes received over socket!\n");
	}

	/* XXX: This won't work since the IO layer needs to update pardat
	 * based on the DR and DDR registers, maybe...
	 * And also if we just got a request bit, then the rest of the data
	 * received might be invalid? vpar_read() may need to be cleaned up for htat*/
	if (pd.refresh) {
		pp_write();
	}

	/* The struct is just a whole bunch of uint8_t's, its probably better
	 * to do a union or maybe just have it be an array to begin with? */
	return (uint8_t *)pd[offs];

}

/* XXX: I think this needs to update EVERYTHING on every write just to nsure
 * a consistent bus state? */
int pp_write(void *handle)
{
	vpar_write(handle, struct pardat *val);
}


#endif // __VPARLIB_H__


