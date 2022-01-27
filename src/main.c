#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "mem.h"
#include "msemu.h"
#include "sizes.h"
#include "ui.h"

#include <SDL2/SDL.h>
#if defined(_MSC_VER)
	#include "platform/windows/getopt.h"
	#include "platform/windows/strndup.h"
#else
	#include <getopt.h>
#endif

void usage(const char *path_arg, const char *cf_path, const char *df_path)
{
	printf(
	  "\nMailstation Emulator\n\n"

	  "Usage: \n"
	  "  %s [-c <path] [-d <path> [-n]] [-l <path>] [POWER_OPTS]\n"
	  "  %s -h | --help\n\n"

	  "  -c <path>, --codeflash <path>  Path to codeflash ROM (def: %s)\n"
	  "  -d <path>, --dataflash <path>  Path to dataflash ROM (def: %s)\n"
	  "  -n                             Don't write dataflash/codeflash changes back to disk\n"
	  "  -r <path>, --ram <path>        Path to RAM image. Meant for pre-loading an image\n"
	  "                                 in to RAM. Image is set in place each time RAM is\n"
	  "                                 normally initialized (e.g. poweron). RAM images are\n"
	  "                                 never written back to disk. If not specified, RAM is\n"
	  "                                 initialized with random data (normal for SRAM).\n"
	  "  -0, --rammod                   Set RAMp1 in Slot0 at power-on-reset. Mimics hardware\n"
	  "                                 modification to allow RAM in Slot0\n"
	  "  -h, --help                     This usage information\n\n"

	  "POWER_OPTS:\n"
	  "  --ac                           Start system with AC power input connected (default)\n"
	  "  --no-ac                        Start system with AC power input removed\n"
	  "  --batt                         Start system with Battery high level (default)\n"
	  "  --low-batt                     Start system with Battery low level\n"
	  "  --no-batt                      Start system with Battery depleted\n\n"

	  "Debugger:\n"
	  "  When running, press ctrl+c on the terminal window to halt exec\n"
	  "  and drop to interactive debug shell. Use the command 'h' while\n"
	  "  in the shell for further help output regarding debugger use\n\n"

	  "Runtime Hotkeys:\n"
	  " [F12]                           Power button\n"
	  " [F1]-[F5]                       The 5 under-LCD function keys\n"
	  " [F6]                            @ key\n"
	  " [F7]                            Size key\n"
	  " [F8]                            Check Spelling key\n"
	  " [F9]                            Get E-Mail key\n"
	  " [Home]                          Main key\n"
	  " [End]                           Back key\n"
	  " [Insert]                        Print key\n"
	  " [L CTRL]                        Function key\n"
	  " [R CTRL] + [r]                  Hard reset\n"
          " [R CTRL] + [b]                  Cycle Battery levels\n"
	  " [R CTRL] + [a]                  Toggle AC adapter connected\n"
	  " [Esc]                           Immediately quit emulator\n",
	  path_arg, path_arg, cf_path, df_path);
}

#define AC		1
#define NO_AC		2
#define BATT		3
#define LOW_BATT	4
#define NO_BATT		5
int main(int argc, char** argv)
{
	int c;
	int ret = 0;

	ms_ctx ms;

	static struct option long_opts[] = {
	  { "help", no_argument, NULL, 'h' },
	  { "codeflash", required_argument, NULL, 'c' },
	  { "dataflash", required_argument, NULL, 'd' },
	  { "ram", required_argument, NULL, 'r' },
	  { "rammod", no_argument, NULL, '0' },
	  { "ac", no_argument, NULL,  AC },
	  { "no-ac", no_argument, NULL, NO_AC },
	  { "batt", no_argument, NULL, BATT },
	  { "low-batt", no_argument, NULL, LOW_BATT },
	  { "no-batt", no_argument, NULL, NO_BATT },
	/* TODO: Add argument to start with debug console open, e.g. execution
	 * halted.
	 */
	  { NULL, no_argument, NULL, 0}
	};

	/* Prepare default options */
	ms_opts options;
	options.cf_path = strndup("codeflash.bin", 13);
	options.df_path = strndup("dataflash.bin", 13);
	options.ram_path = NULL;
	options.ram_mod_por = 0;
	options.df_save_to_disk = 1;
	options.batt_start = BATT_HIGH;
	options.ac_start = AC_GOOD;

	/* Process arguments */
	while ((c = getopt_long(argc, argv,
	  "hc:d:r:0n", long_opts, NULL)) != -1) {
		switch(c) {
		  case 'c':
			options.cf_path = malloc(strlen(optarg)+1);
			/* TODO: Implement error handling here */
			strncpy(options.cf_path, optarg, strlen(optarg)+1);
			break;
		  case 'd':
			options.df_path = malloc(strlen(optarg)+1);
			/* TODO: Implement error handling here */
			strncpy(options.df_path, optarg, strlen(optarg)+1);
			break;
		  case 'r':
			options.ram_path = malloc(strlen(optarg)+1);
			/* TODO: Implement error handling here */
			strncpy(options.ram_path, optarg, strlen(optarg)+1);
			break;
		  case '0':
			options.ram_mod_por = 1;
			break;
		  case 'n':
			options.df_save_to_disk = 0;
			break;
		  case AC:
			options.ac_start = AC_GOOD;
			break;
		  case NO_AC:
			options.ac_start = AC_FAIL;
			break;
		  case BATT:
			options.batt_start = BATT_HIGH;
			break;
		  case LOW_BATT:
			options.batt_start = BATT_LOW;
			break;
		  case NO_BATT:
			options.batt_start = BATT_DEPLETE;
			break;
		  case 'h':
		  default:
			usage(argv[0], options.cf_path, options.df_path);
			return 1;
		}
	}

	// Init mailstation w/ options
	memset(&ms, '\0', sizeof(ms));
	if (ms_init(&ms, &options) == MS_ERR) return 1;
	ui_init(ms.lcd_datRGBA8888);

	// Run mailstation
	ret = ms_run(&ms);
	if (ret) {
		log_error("mailstation existed with code %d.\n", ret);
	}

	ms_deinit(&ms, &options);

	return ret;
}
