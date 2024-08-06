/* SINOWEALTH Genesis Xenon 750 driver (258a:1007) lets you customize the mouse
 * with a config file.
 *
 * Copyright (C) 2024 jokerzmn <jokerzmnvv@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * See LICENSE file for copyright and license details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <libconfig.h>
#include <libusb-1.0/libusb.h>

#include "data.h"
enum { SUCCESS, ERROR };

/* "some_data" struct members represent data, which I did not research, because
 * they are not essential for this driver to work.
 */
struct {
	uint8_t some_data1[2];
	uint8_t active_dpi_mode_count;
	uint8_t some_data2[2];
	uint8_t dpi_value[6];
	uint8_t some_data3[33];
	uint8_t logo_color[6];
	uint8_t some_data4[9];
} dpi_config_info;

struct {
	uint8_t some_data1[2];
	uint8_t poll_rate;
	uint8_t some_data2[2];
	uint8_t dpi_mode;
	uint8_t some_data3[3];
} modes_info;

struct {
	size_t bytes_written;			/* Bytes written in macro buffer. */
	uint16_t num_of_cycles;			/* How many times to loop macro. */
	uint8_t macro[MAX_MACRO_SIZE];
} macro_info;

/* Each element of mouse_btns array correspond to a specific button on mouse:
 * - mouse_btns[0] = left button,
 * - mouse_btns[1] = right button,
 * - mouse_btns[2] = middle button,
 * - mouse_btns[3] = back button (button on the left side of the mouse, closer to the user),
 * - mouse_btns[4] = forward button (the other button on the left side),
 * - mouse_btns[5] = DPI+ button (button on the top, closer to the scroll wheel),
 * - mouse_btns[6] = DPI- button (the other button on the top).
 *
 * This order can't be changed, because that is how buttons info must be ordered in a buffer
 * for data transfer.
 */
struct {
	uint8_t fun;
	uint8_t args[3];
} mouse_btns[NUM_OF_BUTTONS];

static int claim_if(int interface);
static void cleanup(void);
static int convert_str_to_btn_fun(const char *name, int args[], int mouse_btn_index);
static int convert_str_to_btn_index(const char *name, int *mouse_btn_index);
static void deactivate_dpi_mode(int mode);
static void fill_macro_n_btns_fun_buf(unsigned char *buf);
static int find_device(uint16_t ven_id, uint16_t prod_id, uint8_t bus_num, uint8_t port_num);
static void get_btns_fun_config(struct config_setting_t *conf_setting);
static void get_default_config(void);
static void get_dpi_modes_config(struct config_setting_t *conf_setting);
static void get_macro_config(struct config_setting_t *conf_setting);
static void macro_new_entry(uint8_t fun, int delay, int btn_up);
static int read_config_file(const char *path);
static void release_if(int interface);
static void terminate_prog();
static int transfer_config_to_mouse(void);
static int transfer_data(int dir, int value_type, unsigned char *data, uint16_t data_len);
static void u16_change_bytes_order(uint16_t *x);

static libusb_device_handle *dev = NULL;
static int claimed_if = 0;

int main(int argc, char **argv) {
	int ret = 0;
	uint8_t bus_num  = 0;
	uint8_t port_num = 0;

	if (argc <= 1 || argc >= 5 || argc == 3) {
		printf("Usage: %s [ARG...]\n\n", argv[0]);
		printf("%s\n", "Order of the arguments matter and should be placed with order like below.");
		printf("<config_file>\t\t\tpath to the config file\n");
		printf("(optional) <bus_number>\t\tbus number of the mouse\n");
		printf("(optional) <port_number>\tport number of the mouse\n");
		return ERROR;
	}
	if (geteuid() != 0) {
		printf("%s\n", "You need to run the driver as root.");
		return ERROR;
	}
	/* bus_number and port_number arguments were passed. */
	if (argc == 4) {
		bus_num  = (uint8_t)strtol(argv[2], NULL, 10);
		port_num = (uint8_t)strtol(argv[3], NULL, 10);
	}

	signal(SIGINT, terminate_prog);

	if((ret = libusb_init_context(NULL, NULL, 0)) != SUCCESS) {
		printf("%s\n", "Could not init libusb.");
		return ret;
	}
	if ((ret = find_device(VENDOR_ID, PRODUCT_ID, bus_num, port_num)) != SUCCESS) {
		goto error;
	}
	if ((ret = claim_if(1)) != SUCCESS) {
		goto error;
	}
	get_default_config();

	if ((ret = read_config_file(argv[1])) != SUCCESS) {
		goto error;
	}
	ret = transfer_config_to_mouse();

error:
	cleanup();
	return ret;
}

static int claim_if(int interface) {
	int active = 0;

	if ((active = libusb_kernel_driver_active(dev, interface)) == 1) {	
		if (libusb_detach_kernel_driver(dev, interface) != 0) {
			printf("Could not detach kernel driver on interface=%d\n", interface);
			return ERROR;
		}
	}
	else if (active != 0) {
		printf("Could not check if kernel driver is active on interface=%d\n", interface);
		return ERROR;
	}

	if (libusb_claim_interface(dev, interface) != 0) {
		printf("Could not claim interface=%d. Reattaching kernel driver.\n", interface);

		if (libusb_attach_kernel_driver(dev, interface) != 0) {
			printf("Could not reattach kernel driver on interface=%d\n", interface);
		}
		return ERROR;
	}
	claimed_if = 1;
	return SUCCESS;
}

static void cleanup(void) {
	if (claimed_if) {
		release_if(1);
	}
	if (dev != NULL) {
		libusb_close(dev);
	}
	libusb_exit(NULL);
}

/* Use name string to change the functionality of one of the buttons from 
 * mouse_btns array. To choose a button use mouse_btn_i (mouse_btns index).
 */
static int convert_str_to_btn_fun(const char *name, int args[], int mouse_btn_index) {
	int i;
	/* Order of elements must match btn_funs and btn_fun_args elements.
	 * First 10 funs are so placed to not require args from config file, instead
	 * they require hardcoded args (from btn_fun_args).
	 */
	const char *fun_names[] = {
		"left_btn", "right_btn", "middle_btn", "back_btn", "forward_btn",
		"dpi_p_btn", "dpi_n_btn", "dpi_loop", "three_click", "disable",
		"fire_key", "dpi_lock", "key_combination", "macro"
	};

	for (i = 0; i < NUM_OF_BUTTONS_FUNS; ++i) {
		/* Find functionality name from name string in fun_names array. */
		if (strcmp(fun_names[i], name) == 0) {
			/* 4 least significant bits never change. */
			mouse_btns[mouse_btn_index].fun &= 0x0F;
			mouse_btns[mouse_btn_index].fun += btn_funs[i];

			/* First 10 funs do not use args from config file, they require a predefined arg. */
			if (i < 10) {
				mouse_btns[mouse_btn_index].args[0] = btn_fun_args[i];
			}
			/* Last 4 funs require args from config file. */
			else {
				mouse_btns[mouse_btn_index].args[0] = args[0];
				mouse_btns[mouse_btn_index].args[1] = args[1];
				mouse_btns[mouse_btn_index].args[2] = args[2];
			}
			return SUCCESS;
		}
	}
	/* Incorrect functionality name. */
	return ERROR;
}

/* Use name string to get index for one of mouse_btns structs. */
static int convert_str_to_btn_index(const char *name, int *mouse_btn_index) {
	int i;
	/* Do not change order, otherwise function will give incorrect mouse_btn_index.
	 *
	 * Example: If we swap in places "left_btn" with "middle_btn", then
	 * mouse_btn_index = 0 should point to mouse_btns struct for middle button, but
	 * mouse_btns[0] corresponds to left button, so when in config file 
	 * for a button functionality entry, name = "middle_btn", it will change 
	 * functionality of left button, instead of middle button.
	 */
	const char *btn_names[] = {
		"left_btn", "right_btn", "middle_btn", "back_btn", "forward_btn",
		"dpi_p_btn", "dpi_n_btn"
	};

	for (i = 0; i < NUM_OF_BUTTONS; ++i) {
		/* Find a button name from name string in btn_names array. */
		if (strcmp(btn_names[i], name) == 0) {
			*mouse_btn_index = i;
			return SUCCESS;
		}
	}
	/* Incorrect button name. */
	return ERROR;
}

static void deactivate_dpi_mode(int mode) {
	dpi_config_info.active_dpi_mode_count--;
	dpi_config_info.dpi_value[mode - 1] += 0x80;
}

/* Combine macro_info, mouse_btns structs and other data into a buffer.
 * Layout of the buffer (1145 bytes):
 * - header (never changes) (1 byte),
 * - number of macro cycles (2 bytes),
 * - macro buffer (1022 bytes),
 * - buttons functionalities (40 bytes)
 *   (40 bytes and no 28, because there must be 3 additional 
 *   buttons with disabled functionality),
 * - buttons functionalities with default data (40 bytes),
 * - buttons functionalities with default data (40 bytes).
 */
static void fill_macro_n_btns_fun_buf(unsigned char *buf) {
	const size_t zero_padding_size = MAX_MACRO_SIZE - macro_info.bytes_written;
	const size_t btns_fun_size = NUM_OF_BUTTONS * BUTTON_SIZE;
	const size_t dis_btns_size = NUM_OF_UNK_BUTTONS * BUTTON_SIZE;
	const size_t btns_fun_n_dis_btns_size = btns_fun_size + dis_btns_size;

	uint8_t *p_header = &buf[0];
	uint8_t *p_num_of_cycles = &buf[1];
	uint8_t *p_macro = &buf[3];
	uint8_t *p_btns_fun = &buf[1025];
	uint8_t *p_dis_btns = &buf[1053];
	uint8_t *p_rep_btns_fun_n_dis_btns_1 = &buf[1065];
	uint8_t *p_rep_btns_fun_n_dis_btns_2 = &buf[1105];

	*p_header = 0x06;
	memcpy(p_num_of_cycles, &macro_info.num_of_cycles, 2);
	memcpy(p_macro, &macro_info.macro, macro_info.bytes_written);
	memset(p_macro + macro_info.bytes_written, 0, zero_padding_size);
	memcpy(p_btns_fun, &mouse_btns, btns_fun_size);
	memcpy(p_dis_btns, default_btns_fun + btns_fun_size, dis_btns_size);
	memcpy(p_rep_btns_fun_n_dis_btns_1, default_btns_fun, btns_fun_n_dis_btns_size);
	memcpy(p_rep_btns_fun_n_dis_btns_2, default_btns_fun, btns_fun_n_dis_btns_size);
}

/* If user passed bus and port number arguments to the program:
 * 	   - get all usb devices and loop through them until device with
 * 	   	 specified bus and port number is found,
 * 	   - get vendor id and product id of found device,
 * 	   - if vendor id and product id match with mouse's, then
 * 	     mouse was found,
 * 	     if not, then end the program.
 * Otherwise:
 * 	   - get all usb devices and loop through them,
 * 	   - get vendor id and product id for each device,
 * 	   - if vendor id and product id match with mouse's, then
 * 	     mouse was found,
 * 	     if not, then iterate the loop. 
 */
static int find_device(uint16_t ven_id, uint16_t prod_id, uint8_t bus_num, uint8_t port_num) {
	struct libusb_device_descriptor dev_desc;

	libusb_device **device_list = NULL;
	libusb_device *device = NULL;
	uint8_t dev_bus_num  = 0;
	uint8_t dev_port_num = 0;
	int i = 0;

	if (libusb_get_device_list(NULL, &device_list) <= 0) {
		printf("%s\n", "Could not get list of usb devices");
		libusb_free_device_list(device_list, 0);
		return ERROR;
	}

	for (device = device_list[0]; device; device = device_list[i++]) {
		dev_bus_num  = libusb_get_bus_number(device);
		dev_port_num = libusb_get_port_number(device);

		if (libusb_open(device, &dev) != 0) {
			printf("Could not open device (bus=%d, port=%d). Skipping this device.\n", dev_bus_num, dev_port_num);
			continue;
		}

		/* If user specified bus number and port number in program arguments. */
		if (bus_num > 0 && port_num > 0) {
			if (dev_bus_num != bus_num || dev_port_num != port_num) {
				libusb_close(dev);
				dev = NULL;
				continue;
			}
		}

		if (libusb_get_device_descriptor(device, &dev_desc) != 0) {
			printf("Could not open device descriptor (bus=%d, port=%d). Skipping this device.\n", dev_bus_num, dev_port_num);
			libusb_close(dev);
			dev = NULL;
			continue;
		}
		/* Found device with specified ven_id and prod_id. */
		if (dev_desc.idVendor == ven_id && dev_desc.idProduct == prod_id) {
			break;
		}
		else {
			/* device on specified bus and port number is not mouse. */
			if (bus_num > 0 && port_num > 0) {
				printf("Device on bus=%d, port=%d is not mouse.\n", dev_bus_num, dev_port_num);
				libusb_close(dev);
				dev = NULL;
				break;
			}
		}

		libusb_close(dev);
		dev = NULL;
	}
	libusb_free_device_list(device_list, 0);

	if (dev == NULL) {
		printf("%s\n", "Could not find the mouse.");
		return ERROR;
	}
	return SUCCESS;
}

/* Set members in the mouse_btns structs. */
static void get_btns_fun_config(struct config_setting_t *conf_setting) {
	struct config_setting_t *el = NULL;
	const char *btn_name = NULL;
	const char *fun_name = NULL;
	int btns_fun_count = 0;
	int btn_index = 0;
	int args[3];
	int i;

	btns_fun_count = config_setting_length(conf_setting);

	for (i = 0; i < btns_fun_count; ++i) {
		el = config_setting_get_elem(conf_setting, i);

		if (config_setting_lookup_string(el, "name", &btn_name) != CONFIG_TRUE) {
			printf("%s\n", "config file warning: buttons functionalities entry must contain a button name. Skipping this entry.");
			continue;
		}
		if (convert_str_to_btn_index(btn_name, &btn_index) != SUCCESS) {
			printf("config file warning: could not recognize name of a button (\"%s\"). Skipping this entry.\n", btn_name);
			continue;
		}
		
		if (config_setting_lookup_string(el, "fun", &fun_name) != CONFIG_TRUE) {
			printf("%s\n", "config file warning: buttons functionalities entry must contain a function name. Skipping this entry.");
			continue;
		}
		memset(args, 0, 3 * sizeof(int));
		config_setting_lookup_int(el, "arg1", &args[0]);
		config_setting_lookup_int(el, "arg2", &args[1]);
		config_setting_lookup_int(el, "arg3", &args[2]);

		if (convert_str_to_btn_fun(fun_name, args, btn_index) != 0) {
			printf("config file warning: could not recognize name of a function (\"%s\"). Skipping this entry.\n", fun_name);
			continue;
		}
	}
}

/* Fill mouse structs with default data. */
static void get_default_config(void) {
	memcpy(&dpi_config_info, default_dpi_data, sizeof(default_dpi_data));
	memcpy(&modes_info, default_modes_data, sizeof(default_modes_data));
	memcpy(&mouse_btns, default_btns_fun, NUM_OF_BUTTONS * BUTTON_SIZE);
}

/* Set members in dpi_config_info struct. */
static void get_dpi_modes_config(struct config_setting_t *conf_setting) {
	struct config_setting_t *el = NULL;
	int dpi_mode_count = 0;
	int dpi_mode_i = 0;
	int tmp_value = 0;
	int i;

	dpi_mode_count = config_setting_length(conf_setting);

	for (i = 0; i < dpi_mode_count; ++i) {
		el = config_setting_get_elem(conf_setting, i);

		if (config_setting_lookup_int(el, "mode", &dpi_mode_i) != CONFIG_TRUE) {
			printf("%s\n", "config file warning: DPI mode entry must contain a DPI mode index. Skipping this entry.");
			continue;
		}
		if (dpi_mode_i < 1 || dpi_mode_i > 6) {
			printf("config file warning: DPI mode index value must be between 1 and 6 (value=%d). Skipping this entry.\n",
			dpi_mode_i);
			continue;
		}

		if (config_setting_lookup_int(el, "dpi", &tmp_value) == CONFIG_TRUE) {
			if (tmp_value < 100 || tmp_value > 7499) {
				printf("config file warning: mouse was designed to work with DPI values from 100 to 7400 (value=%d).", tmp_value);
				printf(" %s\n", "Skipping this entry.");
				continue;
			}

			/* Mouse only stores thousandth and hundredth part of dpi value, so
			 * when you set dpi to 1260, then only 12 is stored in memory
			 * and dpi value equals 1200.
			 */
			tmp_value = (int)(tmp_value / 100);
			dpi_config_info.dpi_value[dpi_mode_i - 1] = tmp_value;
		}
		if (config_setting_lookup_int(el, "color", &tmp_value) == CONFIG_TRUE) {
			dpi_config_info.logo_color[dpi_mode_i - 1] = tmp_value;
		}
		if (config_setting_lookup_int(el, "active", &tmp_value) == CONFIG_TRUE) {
			if (tmp_value <= 0) {
				deactivate_dpi_mode(dpi_mode_i);
			}
		}
	}
}

/* Set members in macro_info struct. */
static void get_macro_config(struct config_setting_t *conf_setting) {
	struct config_setting_t *entries = NULL;
	struct config_setting_t *el = NULL;
	uint16_t num_of_cycles = 0;
	int macro_entry_count = 0;
	int macro_fun = 0;
	int macro_fun_up = 0;
	int macro_delay = 0;
	int i;

	/* Get number of macro entries. */
	entries = config_setting_lookup(conf_setting, "entries");
	macro_entry_count = config_setting_length(entries);

	if (config_setting_lookup_int(conf_setting, "num_of_cycles", (int *)&num_of_cycles) == CONFIG_TRUE) {
		u16_change_bytes_order(&num_of_cycles);
	}
	macro_info.num_of_cycles = num_of_cycles;

	for (i = 0; i < macro_entry_count; ++i) {
		el = config_setting_get_elem(entries, i);

		if (config_setting_lookup_int(el, "fun", &macro_fun) != CONFIG_TRUE) {
			printf("%s\n", "config file warning: macro entry must contain a function name. Skipping this entry.");
			continue;
		}
		if (config_setting_lookup_int(el, "fun_up", &macro_fun_up) != CONFIG_TRUE) {
			printf("%s\n", "config file warning: macro entry must contain whether a function is up or not. Skipping this entry.");
			continue;
		}
		if (config_setting_lookup_int(el, "delay", &macro_delay) != CONFIG_TRUE) {
			/* Delay after a macro function has to be at least 1ms. */
			macro_delay = 1;
		}

		macro_new_entry(macro_fun, macro_delay, macro_fun_up);
	}
}

/* Append macro entry to macro buffer in macro_info struct.
 * Macro entry either takes 2 or 4 bytes of space.
 * It takes:
 *		- 2 bytes, when delay is < 128ms,
 *		- 4 bytes, when delay is >= 128ms.
 * Delay refers to delay to put after function call and must be between 1ms and 25627ms.
 * When delay is < 128ms, then:
 * 		- byte 1 = delay after function call and whether function is up or down
 * 		  (examples: 
 * 		  	  - function down: press A key,
 * 		  	  - function up: release A key),
 * 		- byte 2 = function to call.
 * When delay is >= 128ms, then:
 * 		- byte 1 = decimal and unit part of delay value and whether function is up or down,
 * 		- byte 2 = function to call,
 * 		- byte 3 = rest part of delay value,
 * 		- byte 4 = footer (always 0x03).
 * To set function as up, add 128 to the delay value (byte 1), otherwise do nothing.
 *
 * Example of a macro:
 * press left button, 50ms delay, release left button, delay 50ms,
 * press left shift, delay 1378ms, press 1 key, delay 100ms,
 * release 1 key, delay 210ms, release left shift, delay 1ms.
 *
 * function codes for used buttons:
 * - left button = 0xF0,
 * - left shift = 0xE1,
 * - 1 key = 0x1E.
 *
 * I will put brackets for each entry to better visualize it:
 * [0x32, 0xF0], [0xB2, 0xF0], [0x4E, 0xE1, 0x0D, 0x03], [0x64, 0x1E],
 * [0x8A, 0x1E, 0x02, 0x03], [0x81, 0xE1].
 */
static void macro_new_entry(uint8_t fun, int delay, int fun_up) {
	uint8_t *p_macro_entry = NULL;

	p_macro_entry = &macro_info.macro[macro_info.bytes_written];

	p_macro_entry[1] = fun;
	p_macro_entry[0] = fun_up * 0x80;

	if (delay < 128) {
		macro_info.bytes_written += 2;
		p_macro_entry[0] += delay;
	}
	else {
		macro_info.bytes_written += 4;
		p_macro_entry[0] += delay % 100;
		p_macro_entry[2] = delay / 100;
		p_macro_entry[3] = 0x03;
	}
}

/* Change specific members of mouse structs according to config file.
 * If there is some missing config in config file, then just keep on
 * executing the function.
 * We previously loaded mouse structs with default data, so there
 * would not be random data when some config is missing.
 */ 
static int read_config_file(const char *path) {
	struct config_t conf;
	struct config_setting_t *dpi_modes_list = NULL;
	struct config_setting_t *btns_fun_list = NULL;
	struct config_setting_t *macro_group = NULL;
	int poll_rate = 0;

	config_init(&conf);
	if (config_read_file(&conf, path) == CONFIG_FALSE) {
		printf("config file error: %s on line %d\n", config_error_text(&conf), config_error_line(&conf));

		config_destroy(&conf);
		return ERROR;
	}

	if (config_lookup_int(&conf, "poll_rate", &poll_rate) == CONFIG_TRUE) {
		modes_info.poll_rate = poll_rate;
	}

	/* Change values of dpi_config_info struct. */
	dpi_modes_list = config_lookup(&conf, "dpi_modes");
	if (dpi_modes_list != NULL) {
		get_dpi_modes_config(dpi_modes_list);
	}

	/* Change values of mouse_btns structs. */
	btns_fun_list = config_lookup(&conf, "buttons_functionalities");
	if (btns_fun_list != NULL) {
		get_btns_fun_config(btns_fun_list);
	}

	/* Change values of macro_info struct. */
	macro_group = config_lookup(&conf, "macro");
	if (macro_group != NULL) {
		get_macro_config(macro_group);
	}

	config_destroy(&conf);
	return SUCCESS;
}


static void release_if(int interface) {
	if (libusb_release_interface(dev, interface) != 0) {
		printf("Could not release interface=%d\n", interface);
	}
	if (libusb_attach_kernel_driver(dev, interface) != 0) {
		printf("Could not reattach kernel driver on interface=%d\n", interface);
	}
}

static void terminate_prog() {
	printf("%s\n", "SIGINT detected. Cleaning up.");
	cleanup();
	exit(0);
}

/* Transfer mouse structs. */
static int transfer_config_to_mouse(void) {
	uint8_t buf[MACRO_N_BTNS_FUN_LEN];
	
	if (transfer_data(DIR_OUT, VALUE_DPI_CONFIG, (unsigned char *)&dpi_config_info, DPI_CONFIG_LEN) != SUCCESS) {
		return ERROR;
	}
	if (transfer_data(DIR_OUT, VALUE_CURRENT_MODES, (unsigned char *)&modes_info, CURRENT_MODES_LEN) != SUCCESS) {
		return ERROR;
	}

	/* Combine macro_info, mouse_btns structs and other data. */
	fill_macro_n_btns_fun_buf(buf);
	if (transfer_data(DIR_OUT, VALUE_MACRO_N_BTNS_FUN, buf, MACRO_N_BTNS_FUN_LEN) != SUCCESS) {
		return ERROR;
	}
	return SUCCESS;
}

static int transfer_data(int dir, int value_type, unsigned char *data, uint16_t data_len) {
	int transferred = 0;
	uint8_t req = 0;
	
	if (dir == DIR_IN) {
		req = REQ_IN;
	}
	else if (dir == DIR_OUT) {
		req = REQ_OUT;
	}

	transferred = libusb_control_transfer(dev, dir, req, value_type, TRANSFER_INDEX, data, data_len, TRANSFER_TIMEOUT);
	if (transferred != data_len) {
		printf("Transfer error (expected=%d, transferred=%d).\n", data_len, transferred);
		return ERROR;
	}
	return SUCCESS;
}

static void u16_change_bytes_order(uint16_t *x) {
	*x = (*x >> 8) | (*x << 8);
}

