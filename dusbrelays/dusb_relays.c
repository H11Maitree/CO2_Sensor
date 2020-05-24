#include <stdio.h>
#include <string.h>
#include <usb.h>
#include "dusb_relays.h"
#include "dusb_relays_priv.h"
#include "protocol.h"

static int dusbr_verbose = 0;

#define IS_VERBOSE()	(dusbr_verbose)

int dusbrelays_init(int verbose)
{
	usb_init();
	dusbr_verbose = verbose;
	return 0;
}

void dusbrelays_shutdown(void)
{
}
/*
static void dusbrelays_initListCtx(struct dusbrelays_list_ctx *ctx)
{
	memset(ctx, 0, sizeof(struct dusbrelays_list_ctx));
}
*/
struct dusbrelays_list_ctx *dusbrelays_allocListCtx(void)
{
	return calloc(1, sizeof(struct dusbrelays_list_ctx));
}

void dusbrelays_freeListCtx(struct dusbrelays_list_ctx *ctx)
{
	if (ctx) {
		free(ctx);
	}
}

/**
 * \brief List instances of our rgbleds device on the USB busses.
 * \param dst Destination buffer for device serial number/id. 
 * \param dstbuf_size Destination buffer size.
 */
dusb_relays_device_t dusbrelays_listDevices(struct dusbrelays_info *info, struct dusbrelays_list_ctx *ctx)
{
	struct usb_bus *start_bus;
	struct usb_device *start_dev;

	memset(info, 0, sizeof(struct dusbrelays_info));

	if (ctx->dev && ctx->bus)
		goto jumpin;

	if (IS_VERBOSE()) {
		printf("Start listing\n");
	}

	usb_find_busses();
	usb_find_devices();
	
	start_bus = usb_get_busses();

	if (start_bus == NULL) {
		if (IS_VERBOSE()) {
			printf("No USB busses found!\n");
		}
		return NULL;
	}

	for (ctx->bus = start_bus; ctx->bus; ctx->bus = ctx->bus->next) {
		
		start_dev = ctx->bus->devices;
		for (ctx->dev = start_dev; ctx->dev; ctx->dev = ctx->dev->next) {
			if (IS_VERBOSE()) {
				printf("Considering USB 0x%04x:0x%04x\n", ctx->dev->descriptor.idVendor, ctx->dev->descriptor.idProduct);
			}
			if (ctx->dev->descriptor.idVendor == OUR_VENDOR_ID) {
				if (ctx->dev->descriptor.idProduct == OUR_PRODUCT_ID) {
					usb_dev_handle *hdl;

					if (IS_VERBOSE()) {
						printf("Recognized 0x%04x:0x%04x\n", ctx->dev->descriptor.idVendor, ctx->dev->descriptor.idProduct);
					}

					info->minor = ctx->dev->descriptor.bcdDevice & 0xff;
					info->major = (ctx->dev->descriptor.bcdDevice & 0xff00) >> 8;
					info->num_relays = 2;

					// Try to read serial number
					hdl = usb_open(ctx->dev);
					if (hdl) {
						info->access = 1;

						if (0 >= usb_get_string_simple(hdl, ctx->dev->descriptor.iProduct, info->str_prodname, 256)) {
							info->access = 0;
						}
						if (0 >= usb_get_string_simple(hdl, ctx->dev->descriptor.iSerialNumber, info->str_serial, 256)) {
							info->access = 0;
						}
						usb_close(hdl);
					}

					return ctx->dev;
				}
			}
			
jumpin:
			// prevent 'error: label at end of compound statement' 
			continue;
		}
	}

	return NULL;
}

dusb_relays_hdl_t dusbrelays_openDevice(dusb_relays_device_t dusb_dev)
{
	struct usb_dev_handle *hdl;
	int res;

	hdl = usb_open(dusb_dev);
	if (!hdl)
		return NULL;

	res = usb_claim_interface(hdl, 0);
	if (res<0) {
		usb_close(hdl);
		return NULL;
	}

	return hdl;
}

void dusbrelays_closeDevice(dusb_relays_hdl_t hdl)
{
	usb_release_interface(hdl, 0);
	usb_close(hdl);
}

static unsigned char xor_buf(unsigned char *buf, int len)
{
	unsigned char x=0;
	while (len--) {
		x ^= *buf;
		buf++;
	}
	return x;
}

int dusbrelays_cmd(dusb_relays_hdl_t hdl, unsigned char cmd, 
										int id, unsigned char *dst)
{
	unsigned char buffer[8];
	unsigned char xor;
	int n, i;
	int datlen;
	static int first = 1, trace = 0;

	if (first) {
		if (getenv("DUSB_RELAYS_TRACE")) {
			trace = 1;
		}
		first = 0;
	}
	

	n =	usb_control_msg(hdl, 
		USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN, /* requesttype */
		cmd, 	/* request*/
		id, 				/* value */
		0, 					/* index */
		(char*)buffer, sizeof(buffer), 5000);

	if (trace) {
		printf("req: 0x%02x, val: 0x%02x, idx: 0x%02x <> %d: ",
			cmd, id, 0, n);
		if (n>0) {
			for (i=0; i<n; i++) {
				printf("%02x ", buffer[i]);
			}
		}
		printf("\n");
	}

	/* Validate size first */
	if (n>8) {
		fprintf(stderr, "Too much data received! (%d)\n", n);
		return -3;
	} else if (n<2) {
		fprintf(stderr, "Not enough data received! (%d)\n", n);
		return -4;	
	}
	
	/* dont count command and xor */
	datlen = n - 2;

	/* Check if reply is for this command */
	if (buffer[0] != cmd) {
		fprintf(stderr, "Invalid reply received (0x%02x)\n", buffer[0]);
		return -5;
	}

	/* Check xor */
	xor = xor_buf(buffer, n);
	if (xor) {
		fprintf(stderr, "Communication corruption occured!\n");
		return -2;
	}

	if (datlen && dst) {
		memcpy(dst, buffer+1, datlen);
	}

	return datlen;
}

int dusbrelays_control_relay(dusb_relays_hdl_t hdl, unsigned char id, unsigned char on)
{
	unsigned char repBuf[8];
	int res;

	res = dusbrelays_cmd(hdl, DUSBR_CONTROL_RELAY, id | on<< 8, repBuf);
	if (res != 1) {
		return -1;
	}
	return repBuf[0];
}

int dusbrelays_control_all_relays_off(dusb_relays_hdl_t hdl)
{
	unsigned char repBuf[8];
	int res;

	res = dusbrelays_cmd(hdl, DUSBR_CONTROL_ALL_RELAYS_OFF, 0, repBuf);
	if (res != 1) {
		return -1;
	}
	return repBuf[0];
}

int dusbrelays_get_relay_status(dusb_relays_hdl_t hdl, unsigned char id)
{
	unsigned char repBuf[8];
	int res;

	res = dusbrelays_cmd(hdl, DUSBR_GET_RELAY_STATUS, id, repBuf);
	if (res != 1) {
		return -1;
	}
	if (repBuf[0] == 255) {
		return -1; // out of range
	}
	return repBuf[0];
}

int dusbrelays_setserial(dusb_relays_hdl_t hdl, const char *new_serial)
{
	unsigned char repBuf[8];
	int i, len;
	int res;

	len = strlen(new_serial);
	if (len != 6) {
		fprintf(stderr, "Serial number must be 6 characters\n");
		return -1;
	}

	for (i=0; i<len; i++) {
		res = dusbrelays_cmd(hdl, DUSBR_SET_SERIAL, (i&0xff) | (new_serial[i]<<8), repBuf);
		if (res!=0) {
			fprintf(stderr, "Error writing character '%c'. (%d)\n", new_serial[i], res);
			return -2;
		}
	}
	
	/* index 0xff means store to eeprom. */
	res = dusbrelays_cmd(hdl, DUSBR_SET_SERIAL, 0xff, repBuf);
	if (res) {
		return -3;
	}
	
	return 0;
}
