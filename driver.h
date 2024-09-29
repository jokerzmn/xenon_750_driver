#ifndef DRIVER_H
#define DRIVER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <libconfig.h>
#include <libusb-1.0/libusb.h>

#define VENDOR_ID  0x258A
#define PRODUCT_ID 0x1007

/* Used for control transfer */
#define DIR_IN 0xA1
#define DIR_OUT	0x21
#define REQ_IN 1
#define REQ_OUT	9
#define VALUE_DPI_CONFIG 0x0304
#define VALUE_MACRO_N_BTN_FUNS 0x0306
#define VALUE_CURRENT_MODES 0x0308
#define TRANSFER_INDEX 0x0001
#define TRANSFER_TIMEOUT 1000

#define NUM_OF_BUTTONS 7
#define NUM_OF_UNK_BUTTONS 3
#define NUM_OF_BUTTON_FUNS 14
#define BUTTON_SIZE 4

#define DPI_CONFIG_LEN 59
#define CURRENT_MODES_LEN 9
#define MACRO_N_BTN_FUNS_LEN 1145
#define MAX_MACRO_SIZE 1022

typedef enum result { 
	SUC,
	ERR,
	ERR_INSUFFICIENT_PERMS,
	ERR_INIT_LIBUSB,
	ERR_CHECK_KERNEL_DRV_ACT,
	ERR_CLAIM_IF,
	ERR_DETACH_KERNEL_DRV,
	ERR_REATTACH_KERNEL_DRV,
	ERR_GET_USB_DEV_LIST,
	ERR_MOUSE_NOT_FOUND,
	ERR_CONFIG,
	ERR_CONFIG_INCORRECT_BTN_NAME,
	ERR_CONFIG_INCORRECT_FUN_NAME,
	ERR_TRANSFER_DATA
} result;

/* "some_data" struct members represent data, which I did not research, because
 * they are not essential for this driver to work.
 */
typedef struct {
	uint8_t some_data1[2];
	uint8_t active_dpi_mode_count;
	uint8_t some_data2[2];
	uint8_t dpi_value[6];
	uint8_t some_data3[33];
	uint8_t logo_color[6];
	uint8_t some_data4[9];
} DpiInfo;

typedef struct {
	size_t bytes_written;			/* Bytes written in macro buffer. */
	uint16_t num_of_cycles;			/* How many times to loop macro. */
	uint8_t macro[MAX_MACRO_SIZE];
} MacroInfo;

typedef struct {
	uint8_t some_data1[2];
	uint8_t poll_rate;
	uint8_t some_data2[2];
	uint8_t dpi_mode;
	uint8_t some_data3[3];
} ModesInfo;

typedef struct {
	uint8_t fun;
	uint8_t args[3];
} MouseBtnInfo;

result claim_if(int interface);
void cleanup(void);
void deactivate_dpi_mode(int mode);
void fill_macro_n_btn_funs_buf(unsigned char *buf);
result find_device(uint16_t ven_id, uint16_t prod_id);
result get_btn_index(const char *btn_name, unsigned int *btn_index);
void get_btns_fun_config(struct config_setting_t *conf_setting);
void get_bus_n_port_num(const char *bus_str, const char *port_str);
void get_config_file_path(char *path);
void get_default_mouse_config(void);
void get_dpi_modes_config(struct config_setting_t *conf_setting);
void get_fun_args_config(struct config_setting_t *el, int args[]);
void get_macro_config(struct config_setting_t *conf_setting);
void macro_new_entry(uint8_t fun, int delay, bool btn_up);
result read_config_file(const char *path);
void release_if(int interface);
result run(void);
result set_btn_fun_n_args(const char *fun_name, int args[], unsigned int btn_index);
void set_dpi_color(int color, int dpi_mode);
result set_dpi_val(int dpi, int dpi_mode);
void terminate(int sig) __attribute__((noreturn));
result transfer_config_to_mouse(void);
result transfer_data(int dir, int req, int value_type, unsigned char *data, uint16_t data_len);
void u16_change_bytes_order(uint16_t *x);
void usage(void);

#endif
