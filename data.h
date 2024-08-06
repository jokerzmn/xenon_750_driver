#define VENDOR_ID  0x258A
#define PRODUCT_ID 0x1007

/* Used for control transfer */
#define DIR_IN 0xA1
#define DIR_OUT	0x21
#define REQ_IN 1
#define REQ_OUT	9
#define VALUE_DPI_CONFIG 0x0304
#define VALUE_MACRO_N_BTNS_FUN 0x0306
#define VALUE_CURRENT_MODES 0x0308
#define TRANSFER_INDEX 0x0001
#define TRANSFER_TIMEOUT 1000

/* Mouse functionalities. */
#define L_BUTTON_FUN 0x10
#define R_BUTTON_FUN 0x10
#define M_BUTTON_FUN 0x10
#define BACK_BUTTON_FUN 0x10
#define FORWARD_BUTTON_FUN 0x10
#define FIRE_KEY_FUN 0x20
#define DPI_P_FUN 0x40
#define DPI_N_FUN 0x40
#define DPI_LOOP_FUN 0x40
#define DPI_LOCK_FUN 0x40
#define THREE_CLICK_FUN 0x30
#define DISABLE_FUN 0x50
#define KEY_COMBINATION_FUN 0x60
#define MACRO_FUN 0x90

/* Mouse has 14 button functionalities in total, where 10 of them do not require 
 * arguments from config file. These arguments are hardcoded. 
 */
#define L_BUTTON_FUN_ARG 0xF0
#define R_BUTTON_FUN_ARG 0xF1
#define M_BUTTON_FUN_ARG 0xF2
#define BACK_BUTTON_FUN_ARG 0xF3
#define FORWARD_BUTTON_FUN_ARG 0xF4
#define DPI_P_FUN_ARG 0x20
#define DPI_N_FUN_ARG 0x40
#define DISABLE_FUN_ARG 0x01
#define THREE_CLICK_FUN_ARG 0x01
#define DPI_LOOP_FUN_ARG 0x00

#define DPI_CONFIG_LEN 59
#define CURRENT_MODES_LEN 9
#define MACRO_N_BTNS_FUN_LEN 1145

#define MAX_MACRO_SIZE 1022
#define NUM_OF_BUTTONS 7
#define NUM_OF_UNK_BUTTONS 3
#define NUM_OF_BUTTONS_FUNS 14
#define BUTTON_SIZE 4

/* Data to transfer to change dpi settings:
 * - byte 3 = number of active dpi modes,
 * - bytes 6-11 = dpi value for each dpi mode,
 * - bytes 45-50 = logo and scroll wheel color for each dpi mode.
 */
static const uint8_t default_dpi_data[] = {
	0x04, 0x00, 0x06, 0x50, 0x00, 0x0c, 0x18, 0x24, 0x30, 0x3c, 0x48, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x04, 0x02, 0x06,
	0x03, 0x01, 0x00, 0x00, 0x20, 0x59, 0x4a, 0x47, 0x31, 0x38, 0x42
};

/* Data to transfer to switch mouse modes:
 * - byte 3 = switch to specific usb polling rate mode,
 * - byte 6 = switch to specific dpi mode.
 */
static const uint8_t default_modes_data[] = {
	0x08, 0x01, 0x03, 0x03, 0x03, 0x01, 0x03, 0x01, 0x00
};

/* Part of data to transfer to change functionalities of buttons.
 *
 * Each button takes 4 bytes to describe itself. 
 * 4 most significant bits of byte 1 describes button's functionality,
 * 4 least significant bits of byte 1 never change,
 * next 3 bytes are used for arguments of functionality.
 *
 * Even though mouse has 7 buttons, there are 3 additional buttons at the end 
 * of buffer with disabled functionality.
 */
static const uint8_t default_btns_fun[] = {
	0x11, 0xf0, 0x00, 0x00, 0x12, 0xf1, 0x00, 0x00, 0x13, 0xf2, 0x00, 0x00, 0x14, 0xf3, 0x00, 0x00, 
	0x15, 0xf4, 0x00, 0x00, 0x46, 0x20, 0x00, 0x00, 0x47, 0x40, 0x00, 0x00, 0x58, 0x01, 0x00, 0x00, 
	0x59, 0x01, 0x00, 0x00, 0x5a, 0x01, 0x00, 0x00
};

static const uint8_t btn_funs[] = {
	L_BUTTON_FUN, R_BUTTON_FUN, M_BUTTON_FUN, BACK_BUTTON_FUN, FORWARD_BUTTON_FUN,
	DPI_P_FUN, DPI_N_FUN, DPI_LOOP_FUN, THREE_CLICK_FUN, DISABLE_FUN,
	FIRE_KEY_FUN, DPI_LOCK_FUN, KEY_COMBINATION_FUN, MACRO_FUN
};

static const uint8_t btn_fun_args[] = {
	L_BUTTON_FUN_ARG, R_BUTTON_FUN_ARG, M_BUTTON_FUN_ARG, BACK_BUTTON_FUN_ARG,
	FORWARD_BUTTON_FUN_ARG, DPI_P_FUN_ARG, DPI_N_FUN_ARG,
	DPI_LOOP_FUN_ARG, THREE_CLICK_FUN_ARG, DISABLE_FUN_ARG
};

