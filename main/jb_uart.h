#pragma once

#include <inttypes.h>

typedef enum {
    JB_POS_A = 15,
    JB_POS_B = 14,
    JB_POS_Z = 13,
    JB_POS_START = 12,
    JB_POS_DU = 11,
    JB_POS_DD = 10,
    JB_POS_DL = 9,
    JB_POS_DR = 8,
    JB_POS_RESET = 7,
    JB_POS_L = 5,
    JB_POS_R = 4,
    JB_POS_CU = 3,
    JB_POS_CD = 2,
    JB_POS_CL = 1,
    JB_POS_CR = 0,
} jb_btn_pos_t;

void jb_uart_setup(void);

void jb_btn_press(jb_btn_pos_t pos);
void jb_btn_release(jb_btn_pos_t pos);
void jb_set_x(int8_t val);
void jb_set_y(int8_t val);
// Call me after state updates
void jb_update_state(void);
void jb_send_info(void);
void jb_send_state(void);