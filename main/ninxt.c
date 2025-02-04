#include "ble.h"
#include "uart.h"
#include "esp_log.h"

// N64 UART conflicts with console logging over UART, so allow easily disabling it
#define ENABLE_UART 0

const char* TAG = "NinXT";

static void on_controller_state(const n64_controller_state_t* state)
{
    ESP_LOGI(TAG, "received controller state");
}

void app_main(void)
{
    n64_ble_register_state_cb(&on_controller_state);
    n64_ble_init();

#if ENABLE_UART
    n64_uart_init();
#endif
}
