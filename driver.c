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

#include "driver.h"
#include "button_funs.h"
#include "default_mouse_data.h"

static uint8_t bus_num;
static uint8_t port_num;
static char *config_file;
static bool claimed_if;
static libusb_device_handle *dev_handle;
static DpiInfo dpi_info;
static MacroInfo macro_info;
static ModesInfo modes_info;
static MouseBtnInfo mouse_btns[NUM_OF_BUTTONS];

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

result
claim_if(int interface)
{
	int active;

	if ((active = libusb_kernel_driver_active(dev_handle, interface)) == 1) {
		if (libusb_detach_kernel_driver(dev_handle, interface) != 0)
			return ERR_DETACH_KERNEL_DRV;
	}
	else if (active != 0)
		return ERR_CHECK_KERNEL_DRV_ACT;

	if (libusb_claim_interface(dev_handle, interface) != 0) {
		if (libusb_attach_kernel_driver(dev_handle, interface) != 0)
			return ERR_REATTACH_KERNEL_DRV;

		return ERR_CLAIM_IF;
	}

	claimed_if = true;
	return SUC;
}

void
cleanup(void)
{
	if (claimed_if)
		release_if(1);
	if (dev_handle)
		libusb_close(dev_handle);

	libusb_exit(NULL);
}

void
deactivate_dpi_mode(int mode)
{
	dpi_info.active_dpi_mode_count--;
	dpi_info.dpi_value[mode - 1] += 0x80;
}

/* Combine macro_info, mouse_btns structs and other data into a buffer.
 * Layout of the buffer (1145 bytes):
 * - header (never changes) (1 byte),
 * - number of macro cycles (2 bytes),
 * - macro buffer (1022 bytes),
 * - button functionalities (40 bytes)
 *   (40 bytes and no 28, because there must be 3 additional 
 *   buttons with disabled functionality),
 * - buttons functionalities with default data (40 bytes),
 * - buttons functionalities with default data (40 bytes).
 */
void
fill_macro_n_btn_funs_buf(unsigned char *buf)
{
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
result
find_device(uint16_t ven_id, uint16_t prod_id)
{
	struct libusb_device_descriptor dev_desc;
	libusb_device **dev_list;
	libusb_device *dev;
	unsigned int i;
	bool bus_n_port;
	ssize_t dev_num;
	uint8_t dev_bus_num, dev_port_num;

	if ((dev_num = libusb_get_device_list(NULL, &dev_list)) <= 0) {
		libusb_free_device_list(dev_list, 0);
		return ERR_GET_USB_DEV_LIST;
	}
	bus_n_port = bus_num && port_num;

	for (i = 0; i < dev_num; ++i) {
		if (dev_handle) {
			libusb_close(dev_handle);
			dev_handle = NULL;
		}
		dev = dev_list[i];
		dev_bus_num  = libusb_get_bus_number(dev);
		dev_port_num = libusb_get_port_number(dev);

		if (libusb_open(dev, &dev_handle) != 0)
			continue;
		if (bus_n_port && (dev_bus_num != bus_num || dev_port_num != port_num))
			continue;
		if (libusb_get_device_descriptor(dev, &dev_desc) != 0)
			continue;
		if (dev_desc.idVendor == ven_id && dev_desc.idProduct == prod_id)
			break;
		else if (bus_n_port) {
			libusb_close(dev_handle);
			dev_handle = NULL;
			break;
		}
		libusb_close(dev_handle);
		dev_handle = NULL;
	}
	libusb_free_device_list(dev_list, 0);
	return (dev_handle) ? SUC : ERR_MOUSE_NOT_FOUND;
}

result
get_btn_index(const char *btn_name, unsigned int *btn_index)
{
	int i;
	BtnIndicesLU *btn;

	for (i = 0; i < NUM_OF_BUTTONS; ++i) {
		btn = &btn_indices_lu[i];

		if (strcmp(*btn->name, btn_name) == 0) {
			*btn_index = btn->index;
			return SUC;
		}
	}
	return ERR_CONFIG_INCORRECT_BTN_NAME;
}

void
get_btns_fun_config(struct config_setting_t *conf_setting)
{
	struct config_setting_t *el;
	const char *btn_name, *fun_name;
	int btns_fun_count, i;
	unsigned int btn_index;
	int args[3];

	btns_fun_count = config_setting_length(conf_setting);

	for (i = 0; i < btns_fun_count; ++i) {
		el = config_setting_get_elem(conf_setting, i);

		if (config_setting_lookup_string(el, "name", &btn_name) != CONFIG_TRUE)
			continue;
		if (get_btn_index(btn_name, &btn_index) != SUC)
			continue;
		if (config_setting_lookup_string(el, "fun", &fun_name) != CONFIG_TRUE)
			continue;

		get_fun_args_config(el, args);
		if (set_btn_fun_n_args(fun_name, args, btn_index) != SUC)
			continue;
	}
}

void
get_bus_n_port_num(const char *bus_str, const char *port_str)
{
	bus_num  = (uint8_t)strtol(bus_str, NULL, 10);
	port_num = (uint8_t)strtol(port_str, NULL, 10);
}

void
get_config_file_path(char *path)
{
	config_file = path;
}

void
get_default_mouse_config(void)
{
	memcpy(&dpi_info, default_dpi_data, sizeof(default_dpi_data));
	memcpy(&modes_info, default_modes_data, sizeof(default_modes_data));
	memcpy(&mouse_btns, default_btns_fun, NUM_OF_BUTTONS * BUTTON_SIZE);
}

void
get_dpi_modes_config(struct config_setting_t *conf_setting)
{
	struct config_setting_t *el;
	int i;
	int dpi_mode_count, dpi_mode;
	int tmp_value;

	dpi_mode_count = config_setting_length(conf_setting);

	for (i = 0; i < dpi_mode_count; ++i) {
		el = config_setting_get_elem(conf_setting, i);

		if (config_setting_lookup_int(el, "mode", &dpi_mode) != CONFIG_TRUE)
			continue;
		if (dpi_mode < 1 || dpi_mode > 6)
			continue;
		if (config_setting_lookup_int(el, "dpi", &tmp_value) == CONFIG_TRUE) {
			if (set_dpi_val(tmp_value, dpi_mode) != SUC)
				continue;
		}
		if (config_setting_lookup_int(el, "color", &tmp_value) == CONFIG_TRUE)
			set_dpi_color(tmp_value, dpi_mode);

		if (config_setting_lookup_int(el, "active", &tmp_value) == CONFIG_TRUE) {
			if (tmp_value <= 0)
				deactivate_dpi_mode(dpi_mode);
		}
	}
}

void
get_fun_args_config(struct config_setting_t *el, int args[])
{
	memset(args, 0, 3 * sizeof(int));
	config_setting_lookup_int(el, "arg1", &args[0]);
	config_setting_lookup_int(el, "arg2", &args[1]);
	config_setting_lookup_int(el, "arg3", &args[2]);
}

void
get_macro_config(struct config_setting_t *conf_setting)
{
	struct config_setting_t *entry_list;
	struct config_setting_t *el;
	int i;
	int entries, fun, delay;
	uint16_t cycles = 0;
	bool fun_up;

	if (config_setting_lookup_int(conf_setting, "num_of_cycles", (int *)&cycles) == CONFIG_TRUE)
		u16_change_bytes_order(&cycles);

	macro_info.num_of_cycles = cycles;

	entry_list = config_setting_lookup(conf_setting, "entries");
	entries = config_setting_length(entry_list);

	for (i = 0; i < entries; ++i) {
		el = config_setting_get_elem(entry_list, i);

		if (config_setting_lookup_int(el, "fun", &fun) != CONFIG_TRUE)
			continue;
		if (config_setting_lookup_bool(el, "fun_up", (int *)&fun_up) != CONFIG_TRUE)
			continue;
		if (config_setting_lookup_int(el, "delay", &delay) != CONFIG_TRUE)
			delay = 1;

		macro_new_entry(fun, delay, fun_up);
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
void
macro_new_entry(uint8_t fun, int delay, bool fun_up)
{
	uint8_t *p_macro_entry = &macro_info.macro[macro_info.bytes_written];

	p_macro_entry[0] = fun_up * 0x80;
	p_macro_entry[1] = fun;

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
result
read_config_file(const char *path)
{
	struct config_t conf;
	struct config_setting_t *conf_stg;
	int poll_rate;

	config_init(&conf);
	if (config_read_file(&conf, path) == CONFIG_FALSE) {
		fprintf(stderr, "config file error: %s on line %d\n", config_error_text(&conf), config_error_line(&conf));
		config_destroy(&conf);
		return ERR_CONFIG;
	}
	if (config_lookup_int(&conf, "poll_rate", &poll_rate) == CONFIG_TRUE)
		modes_info.poll_rate = poll_rate;

	if ((conf_stg = config_lookup(&conf, "dpi_modes")))
		get_dpi_modes_config(conf_stg);

	if ((conf_stg = config_lookup(&conf, "button_functionalities")))
		get_btns_fun_config(conf_stg);

	if ((conf_stg = config_lookup(&conf, "macro")))
		get_macro_config(conf_stg);

	config_destroy(&conf);
	return SUC;
}


void
release_if(int interface)
{
	libusb_release_interface(dev_handle, interface); 
	libusb_attach_kernel_driver(dev_handle, interface);
}

result
run(void)
{
	int ret = 0;

	if (libusb_init_context(NULL, NULL, 0) != SUC)
		return ERR_INIT_LIBUSB;
	if ((ret = find_device(VENDOR_ID, PRODUCT_ID)) != SUC)
		return ret;
	if ((ret = claim_if(1)) != SUC)
		return ret;
	get_default_mouse_config();

	if ((ret = read_config_file(config_file)) != SUC)
		return ret;

	ret = transfer_config_to_mouse();
	return ret;
}

result
set_btn_fun_n_args(const char *fun_name, int args[], unsigned int btn_index)
{
	int i;
	BtnFunsLU *btn_fun;

	for (i = 0; i < NUM_OF_BUTTON_FUNS; ++i) {
		btn_fun = &btn_funs_lu[i];

		if (strcmp(*btn_fun->name, fun_name) == 0) {
			mouse_btns[btn_index].fun &= 0x0F;
			mouse_btns[btn_index].fun += *btn_fun->fun;

			if (*btn_fun->arg != NO_ARG)
				mouse_btns[btn_index].args[0] = *btn_fun->arg;
			else {
				mouse_btns[btn_index].args[0] = (uint8_t)args[0];
				mouse_btns[btn_index].args[1] = (uint8_t)args[1];
				mouse_btns[btn_index].args[2] = (uint8_t)args[2];
			}
			return SUC;
		}
	}
	return ERR_CONFIG_INCORRECT_FUN_NAME;
}

void
set_dpi_color(int color, int dpi_mode)
{
	dpi_info.logo_color[dpi_mode - 1] = color;
}

result
set_dpi_val(int dpi, int dpi_mode)
{
	if (dpi < 100 || dpi > 7499)
		return ERR;

	dpi = (int)(dpi / 100);
	dpi_info.dpi_value[dpi_mode - 1] = dpi;
	return SUC;
}

void
terminate(int sig)
{
	puts("SIGINT detected. Cleaning up.");
	cleanup();
	exit(0);
}

result
transfer_config_to_mouse(void)
{
	uint8_t buf[MACRO_N_BTN_FUNS_LEN];
	
	if (transfer_data(DIR_OUT, REQ_OUT, VALUE_DPI_CONFIG, (unsigned char *)&dpi_info, DPI_CONFIG_LEN) != SUC)
		return ERR_TRANSFER_DATA;
	if (transfer_data(DIR_OUT, REQ_OUT, VALUE_CURRENT_MODES, (unsigned char *)&modes_info, CURRENT_MODES_LEN) != SUC)
		return ERR_TRANSFER_DATA;

	fill_macro_n_btn_funs_buf(buf);
	if (transfer_data(DIR_OUT, REQ_OUT, VALUE_MACRO_N_BTN_FUNS, buf, MACRO_N_BTN_FUNS_LEN) != SUC)
		return ERR_TRANSFER_DATA;

	return SUC;
}

result
transfer_data(int dir, int req, int value_type, unsigned char *data, uint16_t data_len)
{
	int transferred;

	transferred = libusb_control_transfer(dev_handle, dir, req, value_type, TRANSFER_INDEX, data, data_len, TRANSFER_TIMEOUT);
	return (transferred == data_len) ? SUC : ERR_TRANSFER_DATA;
}

void
u16_change_bytes_order(uint16_t *x)
{
	*x = (*x >> 8) | (*x << 8);
}

void
usage(void)
{
	puts("Usage: xenon_driver [ARG...]\n");
	puts("Order of the arguments matter and should be placed with order like below.");
	puts("<config_file>\t\t\tpath to the config file");
	puts("(optional) <bus_number>\t\tbus number of the mouse");
	puts("(optional) <port_number>\tport number of the mouse");
}

int
main(int argc, char *argv[])
{
	int ret = 0;

	if (argc <= 1 || argc >= 5 || argc == 3) {
		usage();
		return ERR;
	}
	if (geteuid() != 0) {
		fputs("You need to run the driver as root.\n", stderr);
		return ERR_INSUFFICIENT_PERMS;
	}
	if (argc == 4)
		get_bus_n_port_num(argv[2], argv[3]);

	signal(SIGINT, terminate);

	get_config_file_path(argv[1]);
	ret = run();
	cleanup();
	return ret;
}

