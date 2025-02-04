#include "ble.h"
#include "esp_log.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

// From ble_store_config.c deep inside nimble source.
// This was in the examples, so I guess I'll use it too.
void ble_store_config_init(void);

static void advertise(void);
static ble_gatt_access_fn on_access_state;

#define GAP_APPEARANCE 0x0000
#define GAP_ROLE 0x00

static char* TAG = "NinXT_BLE";
static char* DEVICE_NAME = "NinXT";

static uint8_t s_addr_type;
static uint8_t s_addr_val[6];

static const ble_uuid16_t s_svc_uuid = BLE_UUID16_INIT(0x1815);
static const ble_uuid128_t s_chr_uuid = BLE_UUID128_INIT(
    0x7b, 0x0e, 0x45, 0x59, 0x46, 0xa5, 0x41, 0xd2, 0xb5, 0x44, 0x2a, 0x71, 0x61, 0x2f, 0x00, 0x00
);
static uint16_t s_chr_handle;

static const struct ble_gatt_svc_def s_gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &s_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &s_chr_uuid.u,
                .access_cb = on_access_state,
                .flags = BLE_GATT_CHR_F_WRITE,
                .val_handle = &s_chr_handle,
            },
            {0}
        }
    },
    {0}
};

static n64_controller_state_cb_t s_state_cb;

inline static void format_addr(char* addr_str, uint8_t addr[])
{
    sprintf(addr_str, "%02X:%02X:%02X:%02X:%02X:%02X", addr[0], addr[1],
            addr[2], addr[3], addr[4], addr[5]);
}

static void print_conn_desc(struct ble_gap_conn_desc* desc)
{
    char addr_str[18] = {0};

    ESP_LOGI(TAG, "connection handle: %d", desc->conn_handle);

    format_addr(addr_str, desc->our_id_addr.val);
    ESP_LOGI(TAG, "device id address: type=%d, value=%s", desc->our_id_addr.type, addr_str);

    format_addr(addr_str, desc->peer_id_addr.val);
    ESP_LOGI(TAG, "peer id address: type=%d, value=%s", desc->peer_id_addr.type, addr_str);

    ESP_LOGI(TAG,
        "conn_itvl=%d, conn_latency=%d, supervision_timeout=%d, "
        "encrypted=%d, authenticated=%d, bonded=%d\n",
        desc->conn_itvl, desc->conn_latency, desc->supervision_timeout,
        desc->sec_state.encrypted, desc->sec_state.authenticated,
        desc->sec_state.bonded);
}

static void on_reset(int reason)
{
    ESP_LOGI(TAG, "Nimble reset, reason: %d", reason);
}

static void on_sync(void)
{
    int rc = 0;
    char addr_str[18] = {0};

    // Ensure we have an address
    rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "device does not have any available bt address!");
        return;
    }

    // Save address type for use in advertising
    rc = ble_hs_id_infer_auto(0, &s_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to infer address type, error code: %d", rc);
        return;
    }

    // Save address for use in advertising
    rc = ble_hs_id_copy_addr(s_addr_type, s_addr_val, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to copy device address, error code: %d", rc);
        return;
    }

    format_addr(addr_str, s_addr_val);
    ESP_LOGI(TAG, "device address: %s", addr_str);

    advertise();
}

static int on_gap_event(struct ble_gap_event* event, void* arg)
{
    int rc = 0;
    struct ble_gap_conn_desc desc;

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
        {
            ESP_LOGI(TAG, "GAP Event: connection %s; status=%d",
                    event->connect.status == 0 ? "established" : "failed",
                    event->connect.status);

            if (event->connect.status == 0) {
                rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
                if (rc != 0) {
                    ESP_LOGE(TAG, "failed to find connection by handle, error code: %d", rc);
                    return rc;
                }

                print_conn_desc(&desc);
            } else {
                advertise();
            }

            break;
        }
        case BLE_GAP_EVENT_DISCONNECT:
        {
            ESP_LOGI(TAG, "GAP Event: disconnected from peer; reason=%d", event->disconnect.reason);
            advertise();
            break;
        }
        case BLE_GAP_EVENT_CONN_UPDATE:
        {
            ESP_LOGI(TAG, "GAP Event: connection updated; status=%d", event->conn_update.status);

            rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
            if (rc != 0) {
                ESP_LOGE(TAG, "failed to find connection by handle, error code: %d", rc);
                return rc;
            }

            print_conn_desc(&desc);
            break;
        }
        case BLE_GAP_EVENT_ADV_COMPLETE:
        {
            ESP_LOGI(TAG, "GAP Event: advertise complete; reason=%d", event->adv_complete.reason);
            advertise();
            break;
        }
        case BLE_GAP_EVENT_NOTIFY_TX:
        {
            if ((event->notify_tx.status != 0) && (event->notify_tx.status != BLE_HS_EDONE)) {
                ESP_LOGI(TAG,
                    "GAP Event: notify event; conn_handle=%d attr_handle=%d "
                    "status=%d is_indication=%d",
                    event->notify_tx.conn_handle, event->notify_tx.attr_handle,
                    event->notify_tx.status, event->notify_tx.indication);
            }

            break;
        }
        case BLE_GAP_EVENT_MTU:
        {
            ESP_LOGI(TAG, "GAP Event: mtu update; conn_handle=%d cid=%d mtu=%d",
                event->mtu.conn_handle, event->mtu.channel_id, event->mtu.value);
            break;
        }
        case BLE_GAP_EVENT_DATA_LEN_CHG:
        {
            ESP_LOGI(TAG, "GAP Event: Data langth change");
            break;
        }
        case BLE_GAP_EVENT_LINK_ESTAB:
        {
            ESP_LOGI(TAG, "GAP Event: Link %s",
                event->link_estab.status == 0 ? "established" : "failed");
            break;
        }
        default:
        {
            ESP_LOGI(TAG, "GAP Event: Unsupported type %d", event->type);
            break;
        }
    }

    return rc;
}

static void on_gatts_register(struct ble_gatt_register_ctxt* ctxt, void* arg)
{
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) {
        case BLE_GATT_REGISTER_OP_SVC:
            ESP_LOGD(TAG, "GATT Register: service = %s, handle = %d",
                ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf), ctxt->svc.handle);
            break;
        case BLE_GATT_REGISTER_OP_CHR:
            ESP_LOGD(TAG, "GATT Register: characteristic = %s, def_handle = %d, val_handle = %d",
                ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf), ctxt->chr.def_handle, ctxt->chr.val_handle);
            break;
        case BLE_GATT_REGISTER_OP_DSC:
            ESP_LOGD(TAG, "GATT Register: descriptor = %s, handle = %d",
                ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf), ctxt->dsc.handle);
            break;
    }
}

static int on_access_state(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt, void* arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR &&
        attr_handle == s_chr_handle &&
        ctxt->om->om_len == sizeof(n64_controller_state_t) && s_state_cb != NULL) {
            n64_controller_state_t state = {0};
            os_mbuf_copydata(ctxt->om, 0, sizeof(n64_controller_state_t), &state);
            s_state_cb(&state);
    }

    return 0;
}

static void advertise(void)
{
    int rc = 0;

    struct ble_hs_adv_fields adv_fields = {0};

    // Set advertisimg flags to generally discoverable and BR/EDR unsupported
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
    adv_fields.appearance = GAP_APPEARANCE;
    adv_fields.appearance_is_present = true;

    // Set role - peripheral
    adv_fields.le_role = GAP_ROLE;
    adv_fields.le_role_is_present = true;

    // Apply
    rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set advertising data, error code: %d", rc);
        return;
    }

    // Set device address
    struct ble_hs_adv_fields rsp_fields = {0};
    rsp_fields.device_addr = s_addr_val;
    rsp_fields.device_addr_type = s_addr_type;
    rsp_fields.device_addr_is_present = true;

    // Apply
    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set scan response data, error code: %d", rc);
        return;
    }

    // Non-connectable and general discoverable
    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    // Actually start advertising now
    rc = ble_gap_adv_start(s_addr_type, NULL, BLE_HS_FOREVER, &adv_params, on_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to start advertising, error code: %d", rc);
        return;
    }
}

static void task_nimble_host(void* arg)
{
    ESP_LOGI(TAG, "task_nimble_host started");

    // Runs until someone calls nimble_port_stop (so, forever, because we don't do that)
    nimble_port_run();

    vTaskDelete(NULL);
}

int n64_ble_init(void)
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
        return -1;
    }

    // Init nimble host
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize nimble stack, error code: %d ",
                 ret);
        return -1;
    }

    // Init nimble GAP
    ble_svc_gap_init();

    // Set GAP device name
    rc = ble_svc_gap_device_name_set(DEVICE_NAME);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set device name to %s, error code: %d",
                 DEVICE_NAME, rc);
        return rc;
    }

    // Set appearance to generic device
    rc = ble_svc_gap_device_appearance_set(GAP_APPEARANCE);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set device appearance, error code: %d", rc);
        return rc;
    }

    // Init nimble GATT
    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(s_gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set gatt service count, error code: %d", rc);
        return rc;
    }

    rc = ble_gatts_add_svcs(s_gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to register gatt service definitions, error code: %d", rc);
        return rc;
    }

    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.gatts_register_cb = on_gatts_register;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_store_config_init();

    nimble_port_freertos_init(task_nimble_host);

    return 0;
}

void n64_ble_register_state_cb(n64_controller_state_cb_t cb)
{
    s_state_cb = cb;
}