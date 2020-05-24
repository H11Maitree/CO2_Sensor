#ifndef _dusbrelays_h__
#define _dusbrelays_h__

#define OUR_VENDOR_ID 	0x289b
#define OUR_PRODUCT_ID 	0x0100

struct dusbrelays_info {
	char str_prodname[256];
	char str_serial[256];
	int major, minor;
	int access; // True unless direct access to read serial/prodname failed due to permissions.
	int num_relays;
};

struct dusbrelays_list_ctx;

typedef void* dusb_relays_hdl_t; // Cast from usb_dev_handle
typedef void* dusb_relays_device_t; // Cast from usb_device

int dusbrelays_init(int verbose);
void dusbrelays_shutdown(void);

struct dusbrelays_list_ctx *dusbrelays_allocListCtx(void);
void dusbrelays_freeListCtx(struct dusbrelays_list_ctx *ctx);
dusb_relays_device_t dusbrelays_listDevices(struct dusbrelays_info *info, struct dusbrelays_list_ctx *ctx);

dusb_relays_hdl_t dusbrelays_openDevice(dusb_relays_device_t dusb_dev);
void dusbrelays_closeDevice(dusb_relays_hdl_t hdl);

int dusbrelays_control_relay(dusb_relays_hdl_t hdl, unsigned char id, unsigned char on);
int dusbrelays_control_all_relays_off(dusb_relays_hdl_t hdl);
int dusbrelays_get_relay_status(dusb_relays_hdl_t hdl, unsigned char id);

int dusbrelays_cmd(dusb_relays_hdl_t hdl, unsigned char cmd, int id, unsigned char *dst);
int dusbrelays_setserial(dusb_relays_hdl_t hdl, const char *new_serial);


#endif // _dusbrelays_h__

