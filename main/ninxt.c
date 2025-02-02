#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "jb_ble.h"
#include "jb_uart.h"

// See https://www.freertos.org/Why-FreeRTOS/FAQs/Memory-usage-boot-times-context#how-big-should-the-stack-be
#define TASK_RX_STACKSIZE 4096
#define TASK_TX_STACKSIZE 4096

// See https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/01-Tasks-and-co-routines/03-Task-priorities
#define TASK_RX_PRIORITY 9
#define TASK_TX_PRIORITY 8

static TaskHandle_t s_task_rx = NULL;
static TaskHandle_t s_task_tx = NULL;

static void task_rx(void* arg)
{
    for (;;) {
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY)) {
            printf("RX\n");
        }
    }
}

static void task_tx(void* arg)
{
    for (;;) {
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY)) {
            // jb_send_info();
            jb_send_state();
        }
    }
}

void app_main(void)
{
    printf("app_main start\n");
    xTaskCreate(task_rx, "task_rx", TASK_RX_STACKSIZE, NULL, TASK_RX_PRIORITY, &s_task_rx);
    xTaskCreate(task_tx, "task_tx", TASK_TX_STACKSIZE, NULL, TASK_TX_PRIORITY, &s_task_tx);

    jb_ble_init();

/*
    printf("Initializing JB_UART, no more printf\n");
    vTaskDelay(10);

    jb_uart_init();
    jb_update_state();

    int8_t x = 0;
    int8_t y = 0;
*/
    for (;;) {
/*
        jb_btn_press(JB_POS_Z);
        jb_set_x(++x);

        jb_update_state();
        xTaskNotifyGive(s_task_tx);
        vTaskDelay(10);

        jb_btn_release(JB_POS_Z);
        jb_set_y(--y);

        jb_update_state();
        xTaskNotifyGive(s_task_tx);
        vTaskDelay(10);
*/

        vTaskDelay(1000);
    }
}
