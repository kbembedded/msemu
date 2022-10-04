#ifndef __IO_H__
#define __IO_H_

#include "msemu.h"

/* List of IO addresses and bit values */

#define KEYBOARD		0x01
#define KBD_ROLCOL7_BIT		(1 << 7)
#define KBD_ROLCOL6_BIT		(1 << 6)
#define KBD_ROLCOL5_BIT		(1 << 5)
#define KBD_ROLCOL4_BIT		(1 << 4)
#define KBD_ROLCOL3_BIT		(1 << 3)
#define KBD_ROLCOL2_BIT		(1 << 2)
#define KBD_ROLCOL1_BIT		(1 << 1)
#define KBD_ROLCOL0_BIT		(1 << 0)

#define MISC2			0x02
#define MISC2_LCD_EN_BIT	(1 << 7)
#define MISC2_CALLID_DAT_BIT	(1 << 6)
#define MISC2_MODEM_EN_BITn	(1 << 5)
#define MISC2_LED_BIT		(1 << 4)
#define MISC2_LCD_CAS_BIT	(1 << 3)
#define MISC2_CALLID_RDY_BIT	(1 << 2)
#define MISC2_KBD_ROW9_BIT	(1 << 1)
#define MISC2_KBD_ROW8_BIT	(1 << 0)

#define IRQ_MASK		0x03
#define IRQ_CALLID_BIT		(1 << 7)
#define IRQ_MODEM_BIT		(1 << 6)
#define IRQ_RTC_MAYBE_BIT	(1 << 5)
#define IRQ_INC_TIME16_BIT	(1 << 4)
#define IRQ_KBD_BIT		(1 << 1)

#define UNKNOWN0x4		0x04
#define SLOT4_PAGE		0x05
#define SLOT4_DEV		0x06
#define SLOT8_PAGE		0x07
#define SLOT8_DEV		0x08

#define MISC9			0x09 /* Printer ctrl, pwr ok, pwr btn */
#define MISC9_AC_GOOD		(1 << 7)
#define MISC9_BATT_HIGH		(1 << 6)
#define MISC9_BATT_GOOD		(1 << 5)
#define MISC9_PWR_BITS		(MISC9_AC_GOOD | MISC9_BATT_HIGH | MISC9_BATT_GOOD)
#define MISC9_PWR_BTN		(1 << 4)
#define MISC9_DIR		0x0A

#define RTC_SEC			0x10 /* BCD, ones place seconds */
#define RTC_10SEC		0x11 /* BCD, tens place seconds */
#define RTC_MIN			0x12 /* BCD, ones place minutes */
#define RTC_10MIN		0x13 /* BCD, tens place minutes */
#define RTC_HR			0x14 /* BCD, ones place hours */
#define RTC_10HR		0x15 /* BCD, tens place hours */
#define RTC_DOW			0x16 /* BCD, day of week */
#define RTC_DOM			0x17 /* BCD, ones place day of month */
#define RTC_10DOM		0x18 /* BCD, tens place day of month */
#define RTC_MON			0x19 /* BCD, ones place month */
#define RTC_10MON		0x1A /* BCD, tens place month */
#define RTC_YR			0x1B /* BCD, ones place years since 1980 */
#define RTC_10YR		0x1C /* BCD, tens place years since 1980 */
#define RTC_CTRL1		0x1D /* Unknown */
#define RTC_CTRL2		0x1E /* Unknown */
#define RTC_CTRL3		0x1F /* Unknown */

#define PRINT_STATUS		0x21 /* Unknown */
#define PRINT_DDR		0x2C
#define PRINT_DR		0x2D

#define UNKNOWN0x28		0x28

/**
 * Initialize/free buffer for IO.
 * io_init can be called multiple times, will re-zero buffer if prev. allocated
 *
 * *ms			- Pointer to ms_ctx struct
 */
int io_init(ms_ctx *ms);
int io_deinit(ms_ctx *ms);

/**
 * Return a byte from the Mailstation IO buffer.
 *
 * *ms			- Pointer to ms_ctx struct
 * absolute_addr	- Address in range of IO, 0x00:0xFF
 */
uint8_t io_read(ms_ctx *ms, unsigned int absolute_addr);

/**
 * Write a byte to the Mailstation IO buffer.
 *
 * *ms			- Pointer to ms_ctx struct
 * absolute_addr	- Address in range of IO, 0x00:0xFF
 * val			- Byte to write
 */
int io_write(ms_ctx *ms, unsigned int absolute_addr, uint8_t val);

#endif // __IO_H__
