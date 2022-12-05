#ifndef __IO_POWER_H__
#define __IO_POWER_H__

#define IO_POWER_IOCTL_BASE	0x0

#define IO_POWER_ON		IO_POWER_IOCTL_BASE+1
#define IO_POWER_ON_HINT	IO_POWER_IOCTL_BASE+1
#define IO_POWER_OFF		IO_POWER_IOCTL_BASE+2
#define IO_POWER_GET		IO_POWER_IOCTL_BASE+3

#define IO_AC_FAIL		IO_POWER_IOCTL_BASE+4
#define IO_AC_GOOD		IO_POWER_IOCTL_BASE+5
#define IO_AC_TOGGLE		IO_POWER_IOCTL_BASE+6
#define IO_AC_GET		IO_POWER_IOCTL_BASE+7

#define IO_BATT_DEPLETE		IO_POWER_IOCTL_BASE+8
#define IO_BATT_LOW		IO_POWER_IOCTL_BASE+9
#define IO_BATT_HIGH		IO_POWER_IOCTL_BASE+10
#define IO_BATT_CYCLE		IO_POWER_IOCTL_BASE+11

#define IO_PWR_BUTTON_PUSHED	IO_POWER_IOCTL_BASE+12
#define IO_PWR_BUTTON_UNPUSHED	IO_POWER_IOCTL_BASE+13
#define IO_POWER_BUTTON_GET	IO_POWER_IOCTL_BASE+14

int io_pwr_init(void *power);
int io_pwr_deinit(void *power);

int io_pwr_on_hint
int io_pwr_on
int io_pwr_off

int io_pwr_button_set
int io_pwr_button

int io_pwr_batt_set()
int io_pwr_batt_cycle()
int io_pwr_ac_set()
int io_pwr_ac_toggle()

#endif // __IO_POWER_H__
