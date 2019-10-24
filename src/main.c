#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include "flashops.h"
#include "logger.h"
#include "msemu.h"
#include "ui.h"

#include <SDL/SDL.h>

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
	  "  -n                             Don't write changes back to disk\n"
	  "  -l <path>, --logfile <path>    Output debug info to <path>\n"
	  "  -v, --verbose                  Output debug info to terminal\n"
	  "  -h, --help                     This usage information\n\n"

	  "Debugger:\n"
	  "  When running, press ctrl+c on the terminal window to halt exec\n"
	  "  and drop to interactive debug shell. Use the command 'h' while\n"
	  "  in the shell for further help output regarding debugger use\n\n",
	  path_arg, path_arg, cf_path, df_path);
}

/* Debug support */
void sigint(int sig, void* data)
{
	MSHW* ms = (MSHW*)data;
	if (ms) {
		ms->debugger_state |= MS_DBG_ON;
	}

	printf("\nReceived SIGINT, interrupting\n");
}

int main(int argc, char** argv)
{
	char* logpath = NULL;
	int c;
	int ret = 0;
	int opt_verbose = 0;
	int df_save_to_disk = 1;

	MSHW ms;

	struct sigaction sigact;

	static struct option long_opts[] = {
	  { "help", no_argument, NULL, 'h' },
	  { "codeflash", required_argument, NULL, 'c' },
	  { "dataflash", required_argument, NULL, 'd' },
	  { "logfile", optional_argument, NULL, 'l' },
	  { "verbose", no_argument, NULL, 'v' },
	/* TODO: Add argument to start with debug console open, e.g. execution
	 * halted.
	 */
	  { NULL, no_argument, NULL, 0}
	};

	/* Prepare default options */
	MsOpts options;
	options.cf_path = strndup("codeflash.bin", 13);
	options.df_path = strndup("dataflash.bin", 13);

	/* Process arguments */
	while ((c = getopt_long(argc, argv,
	  "hc:d:l:vn", long_opts, NULL)) != -1) {
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
		  case 'n':
			df_save_to_disk = 0;
			break;
		  case 'l':
			logpath = malloc(strlen(optarg) + 1);
			/* TODO: Implement error handling here */
			strncpy(logpath, optarg, strlen(optarg) + 1);
		  	break;
		  case 'v':
			opt_verbose = 1;
			break;
		  case 'h':
		  default:
			usage(argv[0], options.cf_path, options.df_path);
			return 1;
		}
	}

	// Initialize logging
	log_init(logpath, opt_verbose);

	// Init mailstation w/ options
	ms_init(&ms, &options);
	ui_init(ms.lcd_dat8bit);

	// Override ctrl+c to drop to debug console
	sigact.sa_handler = sigint;
	sigaction(SIGINT, &sigact, NULL);

	// Run mailstation
	ret = ms_run(&ms);
	if (ret) {
		log_error("mailstation existed with code %d.\n", ret);
	}

	/* Write dataflash buffer to disk if it was modified */
	if (ms.dataflash_updated) {
		if (!df_save_to_disk) {
			log_error("Not writing modified dataflash to disk!\n");
		} else {
			log_error("Writing dataflash buffer to disk\n");
			ret = buftoflash((uint8_t *)ms.dev_map[DF],
			  options.df_path, MEBIBYTE/2);
			if (ret < MEBIBYTE/2) {
				log_error(
				  "Failed writing dataflash, only wrote %d\n",
				  ret);
			}
		}
	}

	log_shutdown();

	return ret;
}
