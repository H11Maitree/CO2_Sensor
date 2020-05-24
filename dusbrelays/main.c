#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>

#include "version.h"
#include "dusb_relays.h"

static void printUsage(void)
{
	printf("./dusbrelays_ctl [OPTION]... [COMMAND]....\n");
	printf("Control tool for Dracal relay boards. Version %s\n", VERSION_STR);
	printf("\n");
	printf("Options:\n");
	printf("  -h           Print help\n");
	printf("  -l           List devices\n");
	printf("  -s serial    Operate on specified device (required unless -f is specified)\n");
	printf("  -f           If no serial is specified, use first device detected.\n");
	printf("\n");
	printf("Commands:\n");
	printf("  relay_on <id>     Enable relay\n");
	printf("  relay_off <id>    Disable relay\n");
	printf("  all_off           Disable all relays\n");
	printf("  get_relay <id>    Get relay status\n");
	printf("  sleep <ms>        Sleep 'ms' milliseconds\n");
	printf("  setserial x       Change serial number to 'x'\n");
}

static int listDevices(void)
{
	int n_found = 0;
	//dusb_relays_hdl_t hdl;
	dusb_relays_device_t cur_dev = NULL;
	struct dusbrelays_list_ctx *listctx; 
	struct dusbrelays_info inf;
	int had_access_error = 0;

	listctx = dusbrelays_allocListCtx();
	while ((cur_dev=dusbrelays_listDevices(&inf, listctx)))
	{
		n_found++;
		printf("Found device '%s', serial '%s', firmware %d.%d", inf.str_prodname, inf.str_serial, inf.major, inf.minor);
		if (!inf.access) {
			printf(" (Warning: No access)");
			had_access_error = 1;
		}
		printf("\n");
	}
	dusbrelays_freeListCtx(listctx);
	printf("%d device(s) found\n", n_found);

	if (had_access_error) {
		printf("\n** Warning! At least one device was not accessible. Please try as root or configure udev to give appropriate permissions.\n");
	}

	return n_found;
}

#define CMD_UNKNOWN		0
#define CMD_RELAY_ON	1
#define CMD_RELAY_OFF	2
#define CMD_SLEEP		3
#define CMD_SET_SERIAL	4
#define CMD_GET_RELAY	5
#define CMD_ALL_OFF		6

int main(int argc, char **argv)
{
	dusb_relays_hdl_t hdl;
	dusb_relays_device_t cur_dev = NULL, dev = NULL;
	struct dusbrelays_list_ctx *listctx; 
	int opt, retval = 0;
	struct dusbrelays_info inf;
	int verbose = 0, use_first = 0;
	int cmd_list = 0;
	char *target_serial = NULL;
	

	while((opt = getopt(argc, argv, "hls:vf")) != -1) {
		switch(opt)
		{
			case 's':
				target_serial = optarg;
				break;
			case 'f':
				use_first = 1;
				break;
			case 'v':
				verbose = 1;
				break;
			case 'h':
				printUsage();
				return 0;
			case 'l':
				cmd_list = 1;
				break;
			default:
				fprintf(stderr, "Unrecognized argument. Try -h\n");
				return -1;
		}
	}


	dusbrelays_init(verbose);

	if (cmd_list) {
		return listDevices();				
	}

	if (!target_serial && !use_first) {
		fprintf(stderr, "A serial number or -f must be used. Try -h for more information.\n");
		return 1;
	}

	listctx = dusbrelays_allocListCtx();
	while ((cur_dev=dusbrelays_listDevices(&inf, listctx)))
	{
		if (target_serial) {
			if (0 == strcmp(inf.str_serial, target_serial)) {
				dev = cur_dev; // Last found will be used
				break;
			}
		}
		else {
			// use_first == 1
			dev = cur_dev; // Last found will be used
			printf("Will use device '%s' serial '%s' version %d.%d\n", inf.str_prodname, inf.str_serial, inf.major, inf.minor);
			break;
		}
	}
	dusbrelays_freeListCtx(listctx);

	if (!dev) {
		if (target_serial) {
			fprintf(stderr, "Device not found\n");
		} else {
			fprintf(stderr, "No device found\n");
		}
		return 1;
	}

	hdl = dusbrelays_openDevice(dev);
	if (!hdl) {
		printf("Error opening device. (Do you have permissions?)\n");
		return 1;
	}

	//
	// Syntax
	//
	// relay_on id
	// relay_off id
	//
	while (optind < argc) {
		int cmd = CMD_UNKNOWN;
		int val_required = 0;
		int res;

		if (0 == strcmp(argv[optind], "relay_on")) {
			cmd = CMD_RELAY_ON;
			val_required = 1;
		} else if (0 == strcmp(argv[optind], "relay_off")) {
			cmd = CMD_RELAY_OFF;
			val_required = 1;
		} else if (0 == strcmp(argv[optind], "sleep")) {
			cmd = CMD_SLEEP;
			val_required = 1;
		} else if (0 == strcmp(argv[optind], "setserial")) {
			cmd = CMD_SET_SERIAL;
			val_required = 1;
		} else if (0 == strcmp(argv[optind], "get_relay")) {
			cmd = CMD_GET_RELAY;
			val_required = 1;
		} else if (0 == strcmp(argv[optind], "all_off")) {
			cmd = CMD_ALL_OFF;
		}

		if (cmd == CMD_UNKNOWN) {
			fprintf(stderr, "Invalid command. Exiting.\n");
			retval = -1;
			break;
		}

		optind++;
		if (optind == argc && val_required) {
			fprintf(stderr, "Missing argument for command. Exiting.\n");
			retval = -1;
			break;
		}
	
		switch (cmd)
		{
			case CMD_RELAY_ON:
			case CMD_RELAY_OFF:
				{
					int id;
					int on = (cmd == CMD_RELAY_ON);
					
					id = atoi(argv[optind]);
					res = dusbrelays_control_relay(hdl, id, on);
					if (res) {
						fprintf(stderr, "Error setting relay %d\n", id);
						break;
					}
					printf("Relay %d -> %d\n", id, on);	
				}
				break;

			case CMD_ALL_OFF:
				{
					res = dusbrelays_control_all_relays_off(hdl);
					if (res) {
						fprintf(stderr, "Error turning relays off\n");
						break;
					}
					printf("All relays set to off.\n");	
				}
				break;

			case CMD_SLEEP:
				{
					int ms = 0;

					ms = atoi(argv[optind]);
					usleep(ms*1000);
				}
				break;

			case CMD_SET_SERIAL:
				dusbrelays_setserial(hdl, argv[optind]);
				break;

			case CMD_GET_RELAY:
				{
					int id, res;

					id = atoi(argv[optind]);
					res = dusbrelays_get_relay_status(hdl, id);
					if (res < 0) {
						fprintf(stderr, "Error reading relay %d status\n", id);
						break;
					}
					printf("Relay %d status: %s\n", id, res ? "On":"Off");
				}
				break;
		}

		optind++;
	}


	dusbrelays_closeDevice(hdl);
	dusbrelays_shutdown();

	return retval;
}



