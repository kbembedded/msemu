
#include <getopt.h>
#include "msemu.h"
#include "rawcga.h"
#include "ui.h"

/* Main
 *
 * TODO:
 *   Support newer SDL versions
 */
int main(int argc, char *argv[])
{
	char *codeflash_path = "codeflash.bin";
	char *dataflash_path = "dataflash.bin";
	char* logpath = NULL;
	int c;
	int silent = 1;

	static struct option long_opts[] = {
	  { "help", no_argument, NULL, 'h' },
	  { "codeflash", required_argument, NULL, 'c' },
	  { "dataflash", required_argument, NULL, 'd' },
	  { "logfile", optional_argument, NULL, 'l' },
	  { "verbose", no_argument, NULL, 'v' },
	  { NULL, no_argument, NULL, 0}
	};

	/* TODO:
	 *   Rework main and break out in to smaller functions.
	 *   Set up buffers and parse files
	 *   Set up SDL calls
	 *   Then get in to loop
	 */

	/* Process arguments */
	while ((c = getopt_long(argc, argv,
	  "hc:d:l:v", long_opts, NULL)) != -1) {
		switch(c) {
		  case 'c':
			codeflash_path = malloc(strlen(optarg)+1);
			/* TODO: Implement error handling here */
			strncpy(codeflash_path, optarg, strlen(optarg)+1);
			break;
		  case 'd':
			dataflash_path = malloc(strlen(optarg)+1);
			/* TODO: Implement error handling here */
			strncpy(dataflash_path, optarg, strlen(optarg)+1);
			break;
		  case 'l':
			logpath = malloc(strlen(optarg) + 1);
			/* TODO: Implement error handling here */
			strncpy(logpath, optarg, strlen(optarg) + 1);
		  	break;
		  case 'v':
			silent = 0;
			break;
		  case 'h':
		  default:
			printf("Usage: %s [-s] [-c <path>] [-d <path>] [-l <path>]\n", argv[0]);
			printf(" -c <path>   | path to codeflash (default: %s)\n", codeflash_path);
			printf(" -d <path>   | path to dataflash (default: %s)\n", dataflash_path);
			printf(" -l <path>   | path to log file\n");
			printf(" -v          | verbose logging\n");
			printf(" -h          | show this usage menu\n");
			return 1;
		}
	}

	// Initialize logging
	log_init(logpath, silent);

	// XXX: You can't actually call this more than once because Z80EM
	//	is globally defined. We need to switch to a z80 emulator that
	//	supports multiple instances.
	struct mshw* ms = mshw_create(codeflash_path, dataflash_path);


	/* TODO: Add git tags to this, because thats neat */
	printf("\nMailstation Emulator v0.1\n");

	// XXX: raw_cga_array can be used directly from the ui code,
	//	we don't need to be passing it in here.
	ui_init(&raw_cga_array[0], ms->lcd_dat8bit);

	mshw_register_ui_hooks(ms, &ui_process_input, &ui_drawLCD);

	ui_run();

	printf("\nShutting down...\n");

	log_shutdown();

	return 0;
}
