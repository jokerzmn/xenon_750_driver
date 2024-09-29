#ifndef BUTTON_FUNS_H
#define BUTTON_FUNS_H

#include "driver.h"

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
#define NO_ARG 0xFF

typedef struct {
	const int index;
	const char **name;
} BtnIndicesLU;

typedef struct {
	const char **name;
	const uint8_t *fun;
	const uint8_t *arg;
} BtnFunsLU;

const char *btn_names[NUM_OF_BUTTONS] = {
	"left_btn", "right_btn", "middle_btn", "back_btn", "forward_btn",
	"dpi_p_btn", "dpi_n_btn"
};

const char *btn_fun_names[NUM_OF_BUTTON_FUNS] = {
	"left_btn", "right_btn", "middle_btn", "back_btn", "forward_btn",
	"dpi_p_btn", "dpi_n_btn", "dpi_loop", "three_click", "disable",
	"fire_key", "dpi_lock", "key_combination", "macro"
};

const uint8_t btn_funs_data[NUM_OF_BUTTON_FUNS] = {
	L_BUTTON_FUN, R_BUTTON_FUN, M_BUTTON_FUN, BACK_BUTTON_FUN, FORWARD_BUTTON_FUN,
	DPI_P_FUN, DPI_N_FUN, DPI_LOOP_FUN, THREE_CLICK_FUN, DISABLE_FUN,
	FIRE_KEY_FUN, DPI_LOCK_FUN, KEY_COMBINATION_FUN, MACRO_FUN
};

const uint8_t btn_fun_args_data[NUM_OF_BUTTON_FUNS] = {
	L_BUTTON_FUN_ARG, R_BUTTON_FUN_ARG, M_BUTTON_FUN_ARG, BACK_BUTTON_FUN_ARG,
	FORWARD_BUTTON_FUN_ARG, DPI_P_FUN_ARG, DPI_N_FUN_ARG,
	DPI_LOOP_FUN_ARG, THREE_CLICK_FUN_ARG, DISABLE_FUN_ARG, NO_ARG, NO_ARG, NO_ARG, NO_ARG
};

/* Each button name corresponds to a specific index of mouse_btns struct. */
BtnIndicesLU btn_indices_lu[NUM_OF_BUTTONS] = {
	{ 0, &btn_names[0] },
	{ 1, &btn_names[1] },
	{ 2, &btn_names[2] },
	{ 3, &btn_names[3] },
	{ 4, &btn_names[4] },
	{ 5, &btn_names[5] },
	{ 6, &btn_names[6] }
};

/* Each function name corresponds to a specific element of the btn_funs_data and btn_fun_args_data arrays. */ 
BtnFunsLU btn_funs_lu[NUM_OF_BUTTON_FUNS] = {
	{ &btn_fun_names[0], &btn_funs_data[0], &btn_fun_args_data[0] },
	{ &btn_fun_names[1], &btn_funs_data[1], &btn_fun_args_data[1] },
	{ &btn_fun_names[2], &btn_funs_data[2], &btn_fun_args_data[2] },
	{ &btn_fun_names[3], &btn_funs_data[3], &btn_fun_args_data[3] },
	{ &btn_fun_names[4], &btn_funs_data[4], &btn_fun_args_data[4] },
	{ &btn_fun_names[5], &btn_funs_data[5], &btn_fun_args_data[5] },
	{ &btn_fun_names[6], &btn_funs_data[6], &btn_fun_args_data[6] },
	{ &btn_fun_names[7], &btn_funs_data[7], &btn_fun_args_data[7] },
	{ &btn_fun_names[8], &btn_funs_data[8], &btn_fun_args_data[8] },
	{ &btn_fun_names[9], &btn_funs_data[9], &btn_fun_args_data[9] },
	{ &btn_fun_names[10], &btn_funs_data[10], &btn_fun_args_data[10] },
	{ &btn_fun_names[11], &btn_funs_data[11], &btn_fun_args_data[11] },
	{ &btn_fun_names[12], &btn_funs_data[12], &btn_fun_args_data[12] },
	{ &btn_fun_names[13], &btn_funs_data[13], &btn_fun_args_data[13] }
};

#endif
