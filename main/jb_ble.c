#include <stdio.h>
#include "esp_log.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"
#include "services/gap/ble_svc_gap.h"
#include "jb_ble.h"

// From ble_store_config.c deep inside nimble source.
// This was in the examples, so I guess I'll use it too.
void ble_store_config_init(void);

static void advertise(void);

#define JB_GAP_APPEARANCE 0x0000
#define JB_GAP_ROLE 0x00

static char* TAG = "NinXT";
static char* DEVICE_NAME = "NinXT";

// our address type
static uint8_t own_addr_type;
// our address value
static uint8_t addr_val[6] = {0};

inline static void format_addr(char *addr_str, uint8_t addr[]) {
    sprintf(addr_str, "%02X:%02X:%02X:%02X:%02X:%02X", addr[0], addr[1],
            addr[2], addr[3], addr[4], addr[5]);
}

static void on_reset(int reason)
{
    ESP_LOGI(TAG, "Nimble reset, reason: %d", reason);
}

static void on_sync(void)
{
    ESP_LOGI(TAG, "Nimble sync");

    int rc = 0;
    char addr_str[18] = {0};

    // Ensure we have an address
    rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "device does not have any available bt address!");
        return;
    }

    // Save address type for use in advertising
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to infer address type, error code: %d", rc);
        return;
    }

    // Save address for use in advertising
    rc = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to copy device address, error code: %d", rc);
        return;
    }

    format_addr(addr_str, addr_val);
    ESP_LOGI(TAG, "device address: %s", addr_str);

    advertise();
}

// TODO: This will be called from GATT event loop as well
static void advertise(void)
{
    int rc = 0;

    struct ble_hs_adv_fields adv_fields = {0};

    // Set advertisimg flags
    // TODO: What do these mean? Documentation is shit
    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    // Set device name
    const char* name = ble_svc_gap_device_name();
    adv_fields.name = (uint8_t*)name;
    adv_fields.name_len = strlen(name);
    adv_fields.name_is_complete = true;

    // Set TX power level
    adv_fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    adv_fields.tx_pwr_lvl_is_present = true;

    // Set appearance - generic device
    adv_fields.appearance = JB_GAP_APPEARANCE;
    adv_fields.appearance_is_present = true;

    // Set role - peripheral
    adv_fields.le_role = JB_GAP_ROLE;
    adv_fields.le_role_is_present = true;

    // Apply
    rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set advertising data, error code: %d", rc);
        return;
    }

    // Set device address
    struct ble_hs_adv_fields rsp_fields = {0};
    rsp_fields.device_addr = addr_val;
    rsp_fields.device_addr_type = own_addr_type;
    rsp_fields.device_addr_is_present = true;

    // Apply
    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set scan response data, error code: %d", rc);
        return;
    }

    // Non-connectable and general discoverable
    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_NON;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    // Actually start advertising now
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, NULL, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to start advertising, error code: %d", rc);
        return;
    }

    ESP_LOGI(TAG, "Advertising started");
}

static void task_nimble_host(void* arg)
{
    ESP_LOGI(TAG, "task_nimble_host started");

    // Runs until someone calls nimble_port_stop (so, forever, because we don't do that)
    nimble_port_run();

    vTaskDelete(NULL);
}

void jb_ble_init(void)
{
    int rc = 0;
    esp_err_t ret = ESP_OK;

    // Init NVS flash (Rerequisite for nimble integration)
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize nvs flash, error code: %d ", ret);
        return;
    }

    // Init nimble host
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize nimble stack, error code: %d ",
                 ret);
        return;
    }

    // Init nimble GAP
    ble_svc_gap_init();

    // Set GAP device name
    rc = ble_svc_gap_device_name_set(DEVICE_NAME);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set device name to %s, error code: %d",
                 DEVICE_NAME, rc);
        return;
    }

    // Set appearance to generic device
    rc = ble_svc_gap_device_appearance_set(JB_GAP_APPEARANCE);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set device appearance, error code: %d", rc);
        return;
    }

    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_store_config_init();

    nimble_port_freertos_init(task_nimble_host);
}