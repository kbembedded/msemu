#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "flashops.h"
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
	  "  %s [-c <path] [-d <path> [-n]] [-l <path>] [-v]\n"
	  "  %s -h | --help\n\n"

	  "Options:\n"
	  "  -c <path>, --codeflash <path>  Path to codeflash ROM (def: %s)\n"
	  "  -d <path>, --dataflash <path>  Path to dataflash ROM (def: %s)\n"
	  "  -r <path>, --ram <path>        Path to RAM image. Meant for pre-loading an image\n"
	  "                                 in to RAM. Image is set in place each time RAM is\n"
	  "                                 normally initialized (e.g. poweron). RAM images are\n"
	  "                                 never written back to disk. If not specified, RAM is\n"
	  "                                 initialized with random data (normal for SRAM).\n"
	  "  -n                             Don't write dataflash/codeflash changes back to disk\n"
	  "  -h, --help                     This usage information\n\n"

	  "Debugger:\n"
	  "  When running, press ctrl+c on the terminal window to halt exec\n"
	  "  and drop to interactive debug shell. Use the command 'h' while\n"
	  "  in the shell for further help output regarding debugger use\n\n",
	  path_arg, path_arg, cf_path, df_path);
}

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
	options.df_save_to_disk = 1;

	/* Process arguments */
	while ((c = getopt_long(argc, argv,
	  "hc:d:r:n", long_opts, NULL)) != -1) {
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
		  case 'n':
			options.df_save_to_disk = 0;
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
