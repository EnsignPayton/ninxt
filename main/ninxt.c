#include <string.h>
#include "ble.h"
#include "uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ENABLE_BLE  1
#define ENABLE_UART 1

const char* TAG = "NinXT";

#if ENABLE_BLE
static void on_controller_state(const n64_controller_state_t* state)
{
    ESP_LOGI(TAG, "received controller state %#06x %d %d", state->buttons, state->x_axis, state->y_axis);

    n64_uart_set_state(state);
}
#endif

void app_main(void)
{
#if ENABLE_BLE
    n64_ble_register_state_cb(&on_controller_state);
    n64_ble_init();
#else
    ESP_LOGI(TAG, "BLE Disabled");
#endif

#if ENABLE_UART
    ESP_LOGI(TAG, "Initializing UART - logging will break now!");
    vTaskDelay(10);
    n64_uart_init();
#else
    ESP_LOGI(TAG, "UART Disabled");
#endif
}
