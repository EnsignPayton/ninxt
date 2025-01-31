#include "jb_uart.h"
#include "sdkconfig.h"
#include "driver/uart.h"
#include "soc/clk_tree_defs.h"

// Resources
// https://www.qwertymodo.com/hardware-projects/n64/n64-controller
// https://n64brew.dev/wiki/Joybus_Protocol
// https://n64brew.dev/wiki/Controller
// https://ctrlsrc.io/posts/2023/gpio-speed-esp32c3-esp32c6/

#define JB_UART_RX 44
#define JB_UART_TX 43
#define JB_UART_PORT_NUM UART_NUM_1
#define JB_UART_BAUD_RATE 1000000

// UART sees 10 bits (start + byte + stop) for evert 2 bits of JB data
// UART is LSB, JB is MSB
//
// 0b00 ____|____|____|----|____|____|____|----|
// 0x08 ___|___|___|___|---|___|___|___|___|---|
#define JB00 0x08
// 0b01 ____|____|____|----|____|----|----|----|
// 0xE8 ___|___|___|___|---|___|---|---|---|---|
#define JB01 0xE8
// 0b10 ____|----|----|----|____|____|____|----|
// 0x0F ___|---|---|---|---|___|___|___|___|---|
#define JB10 0x0F
// 0b11 ____|----|----|----|____|----|----|----|
// 0xEF ___|---|---|---|---|___|---|---|---|---|
#define JB11 0xEF
// We also send a stop bit at end of transmission.
// STOP ____|____|----|----|----|----|----|----|
// 0xFC ___|___|___|---|---|---|---|---|---|---|
#define JBSTOP 0xFC

// 0x050000
#define JB_INFO_SIZE 13
const uint8_t s_jb_info[JB_INFO_SIZE] = {
    JB00, JB00, JB01, JB01, JB00, JB00, JB00, JB00, JB00, JB00, JB00, JB00, JBSTOP
};

typedef struct {
    uint16_t btn_flags;
    int8_t stick_x;
    int8_t stick_y;
} jb_con_state_t;

// Source of truth for current controller state
jb_con_state_t s_jb_con_state;

#define JB_STATE_SIZE 17
uint8_t s_jb_con_state_data[JB_STATE_SIZE];

uint8_t _jb_pair_to_uart(uint8_t value)
{
    switch (value & 0b11)
    {
        default:
        case 0b00:
            return JB00;
        case 0b01:
            return JB01;
        case 0b10:
            return JB10;
        case 0b11:
            return JB11;
    }
}

void jb_uart_setup(void)
{
    uart_config_t uart_config = {
        .baud_rate = JB_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_driver_install(JB_UART_PORT_NUM, 2048, 0, 0, NULL, 0);
    uart_param_config(JB_UART_PORT_NUM, &uart_config);
    uart_set_pin(JB_UART_PORT_NUM, JB_UART_TX, JB_UART_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

void jb_btn_press(jb_btn_pos_t pos)
{
    s_jb_con_state.btn_flags |= (1 << pos);
}

void jb_btn_release(jb_btn_pos_t pos)
{
    s_jb_con_state.btn_flags &= ~(1 << pos);
}

void jb_set_x(int8_t val)
{
    s_jb_con_state.stick_x = val;
}

void jb_set_y(int8_t val)
{
    s_jb_con_state.stick_y = val;
}

// Call me after state updates
void jb_update_state(void)
{
    // TODO: Probably take a mutex on s_jb_con_state_data
    s_jb_con_state_data[0] = _jb_pair_to_uart(
        ((s_jb_con_state.btn_flags >> (JB_POS_A - 1)) & 0b10) |
        ((s_jb_con_state.btn_flags >> JB_POS_B) & 0b01));
    s_jb_con_state_data[1] = _jb_pair_to_uart(
        ((s_jb_con_state.btn_flags >> (JB_POS_Z - 1)) & 0b10) |
        ((s_jb_con_state.btn_flags >> JB_POS_START) & 0b01));
    s_jb_con_state_data[2] = _jb_pair_to_uart(
        ((s_jb_con_state.btn_flags >> (JB_POS_DU - 1)) & 0b10) |
        ((s_jb_con_state.btn_flags >> JB_POS_DD) & 0b01));
    s_jb_con_state_data[3] = _jb_pair_to_uart(
        ((s_jb_con_state.btn_flags >> (JB_POS_DL - 1)) & 0b10) |
        ((s_jb_con_state.btn_flags >> JB_POS_DR) & 0b01));
    s_jb_con_state_data[4] = _jb_pair_to_uart(
        ((s_jb_con_state.btn_flags >> (JB_POS_DU - 1)) & 0b10) |
        ((s_jb_con_state.btn_flags >> JB_POS_DD) & 0b01));
    s_jb_con_state_data[5] = _jb_pair_to_uart(
        ((s_jb_con_state.btn_flags >> (JB_POS_RESET - 1)) & 0b10));
    s_jb_con_state_data[6] = _jb_pair_to_uart(
        ((s_jb_con_state.btn_flags >> (JB_POS_CU - 1)) & 0b10) |
        ((s_jb_con_state.btn_flags >> JB_POS_CD) & 0b01));
    s_jb_con_state_data[7] = _jb_pair_to_uart(
        ((s_jb_con_state.btn_flags >> (JB_POS_CL - 1)) & 0b10) |
        ((s_jb_con_state.btn_flags >> JB_POS_CR) & 0b01));
    s_jb_con_state_data[8] = _jb_pair_to_uart(s_jb_con_state.stick_x >> 6);
    s_jb_con_state_data[9] = _jb_pair_to_uart(s_jb_con_state.stick_x >> 4);
    s_jb_con_state_data[10] = _jb_pair_to_uart(s_jb_con_state.stick_x >> 2);
    s_jb_con_state_data[11] = _jb_pair_to_uart(s_jb_con_state.stick_x >> 0);
    s_jb_con_state_data[12] = _jb_pair_to_uart(s_jb_con_state.stick_y >> 6);
    s_jb_con_state_data[13] = _jb_pair_to_uart(s_jb_con_state.stick_y >> 4);
    s_jb_con_state_data[14] = _jb_pair_to_uart(s_jb_con_state.stick_y >> 2);
    s_jb_con_state_data[15] = _jb_pair_to_uart(s_jb_con_state.stick_y >> 0);
    s_jb_con_state_data[16] = JBSTOP;
}

void jb_send_info(void)
{
    uart_write_bytes(JB_UART_PORT_NUM, (const char *) s_jb_info, JB_INFO_SIZE);
}

void jb_send_state(void)
{
    // TODO: Probably take a mutex on s_jb_con_state_data
    uart_write_bytes(JB_UART_PORT_NUM, (const char *) s_jb_con_state_data, JB_STATE_SIZE);
}