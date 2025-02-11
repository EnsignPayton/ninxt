#include "uart.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static char* TAG = "NinXT_UART";

#define JB_UART_TX 43
#define JB_UART_RX 44
#define JB_UART_PORT_NUM UART_NUM_1
#define JB_UART_BAUD_RATE 1000000
#define RX_BUF_SIZE 64

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
const uint8_t s_info_arr[JB_INFO_SIZE] = {
    JB00, JB00, JB01, JB01, JB00, JB00, JB00, JB00, JB00, JB00, JB00, JB00, JBSTOP
};

#define JB_STATE_SIZE 17
static uint8_t s_state_arr[JB_STATE_SIZE];

static n64_controller_state_t s_state;

static uint8_t to_uart(uint8_t value)
{
    switch (value & 0b11) {
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

static void fill_state_arr()
{
    for (int i = 0; i < 8; i++) {
        s_state_arr[i] = to_uart(s_state.buttons >> (2 * (7 - i)));
    }

    for (int i = 0; i < 4; i++) {
        s_state_arr[8 + i] = to_uart(s_state.x_axis >> (2 * (3 - i)));
    }

    for (int i = 0; i < 4; i++) {
        s_state_arr[12 + i] = to_uart(s_state.y_axis >> (2 * (3 - i)));
    }

    s_state_arr[16] = JBSTOP;
}

static void uart_task(void* arg)
{
    uint8_t* data = (uint8_t*)malloc(RX_BUF_SIZE + 1);
    for (;;) {
        const int bytes_read = uart_read_bytes(JB_UART_PORT_NUM, data, RX_BUF_SIZE, 0);
        if (bytes_read > 0) {
            // Echo
            uart_write_bytes(JB_UART_PORT_NUM, (const char*)data, bytes_read);
        }
    }

    free(data);
}

int n64_uart_init(void)
{
    gpio_set_pull_mode(JB_UART_RX, GPIO_FLOATING);

    esp_err_t ret;
    uart_config_t uart_config = {
        .baud_rate = JB_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ret = uart_param_config(JB_UART_PORT_NUM, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to configure uart");
        return 1;
    }

    ret = uart_set_pin(JB_UART_PORT_NUM, JB_UART_TX, JB_UART_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to set uart pins");
        return 1;
    }

    ret = uart_driver_install(JB_UART_PORT_NUM, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to install uart driver");
        return 1;
    }

    xTaskCreate(&uart_task, "uart_task", configMINIMAL_STACK_SIZE, NULL, 12, NULL);

    return 0;
}

void n64_uart_set_state(n64_controller_state_t* state)
{
    // TODO: Thread safety
    s_state.buttons = state->buttons;
    s_state.x_axis = state->x_axis;
    s_state.y_axis = state->y_axis;
    fill_state_arr();
}
