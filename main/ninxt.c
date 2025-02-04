#include <string.h>
#include "ble.h"
#include "uart.h"
#include "esp_log.h"

// N64 UART conflicts with console logging over UART, so allow easily disabling it
#define ENABLE_UART 0

const char* TAG = "NinXT";

static void on_controller_state(const n64_controller_state_t* state)
{
    if (state->buttons & 0x8000) {
        ESP_LOGI(TAG, "A");
    }
    if (state->buttons & 0x4000) {
        ESP_LOGI(TAG, "B");
    }
    if (state->buttons & 0x2000) {
        ESP_LOGI(TAG, "Z");
    }
    if (state->buttons & 0x1000) {
        ESP_LOGI(TAG, "S");
    }

    if (state->buttons & 0x0800) {
        ESP_LOGI(TAG, "DU");
    }
    if (state->buttons & 0x0400) {
        ESP_LOGI(TAG, "DD");
    }
    if (state->buttons & 0x0200) {
        ESP_LOGI(TAG, "DL");
    }
    if (state->buttons & 0x0100) {
        ESP_LOGI(TAG, "DR");
    }

    if (state->buttons & 0x0020) {
        ESP_LOGI(TAG, "L");
    }
    if (state->buttons & 0x0010) {
        ESP_LOGI(TAG, "R");
    }

    if (state->buttons & 0x0008) {
        ESP_LOGI(TAG, "CU");
    }
    if (state->buttons & 0x0004) {
        ESP_LOGI(TAG, "CD");
    }
    if (state->buttons & 0x0002) {
        ESP_LOGI(TAG, "CL");
    }
    if (state->buttons & 0x0001) {
        ESP_LOGI(TAG, "CR");
    }

    ESP_LOGI(TAG, "received controller state %#06x %d %d", state->buttons, state->x_axis, state->y_axis);
}

void app_main(void)
{
    n64_ble_register_state_cb(&on_controller_state);
    n64_ble_init();

#if ENABLE_UART
    n64_uart_init();
#endif
}
