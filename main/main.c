
#include "common.h"
// #include "gap.h"
#include "host/ble_gap.h"
#include "services/gap/ble_svc_gap.h"

/* Defines */
#define BLE_GAP_APPEARANCE_GENERIC_TAG 0x0200
#define BLE_GAP_URI_PREFIX_HTTPS 0x17
#define BLE_GAP_LE_ROLE_PERIPHERAL 0x00
/* Library function declarations */
void ble_store_config_init(void);

/* Private function declarations */
static void nimble_host_config_init(void);
static void nimble_host_task(void *param);

/* Private functions */

inline static void format_addr(char *addr_str, uint8_t addr[]) {
    sprintf(addr_str, "%02X:%02X:%02X:%02X:%02X:%02X", addr[0], addr[1],
            addr[2], addr[3], addr[4], addr[5]);
}

static void set_random_addr(void) {
    /* Local variables */
    int rc = 0;
    ble_addr_t addr;

    /* Generate new non-resolvable private address */
    rc = ble_hs_id_gen_rnd(0, &addr);
    assert(rc == 0);

    /* Set address */
    rc = ble_hs_id_set_rnd(addr.val);
    assert(rc == 0);
}

static int advertiser_cb(struct ble_gap_event *event, void *arg){
    ESP_LOGI(TAG, "Advertiser callback");
    return 0;
}

int gap_init(void) {
    /* Local variables */
    int rc = 0;

    /* Call NimBLE GAP initialization API */
    ble_svc_gap_init();

    /* Set GAP device name */
    rc = ble_svc_gap_device_name_set(DEVICE_NAME);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set device name to %s, error code: %d",
                 DEVICE_NAME, rc);
        return rc;
    }
    return rc;
}

void start_advertising(void) {
    /* Local variables */
    int rc = 0;
    const char *name;
    struct ble_hs_adv_fields adv_fields = {0};
    struct ble_gap_adv_params adv_params = {0};

    /* Set advertising flags */
    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    /* Set device name */
    name = "placeholder";
    adv_fields.name = (uint8_t *)name;
    adv_fields.name_len = strlen(name);
    adv_fields.name_is_complete = 1;

    /* Set device tx power */
    adv_fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    adv_fields.tx_pwr_lvl_is_present = 1;

    /* Set device appearance */
    adv_fields.appearance = BLE_GAP_APPEARANCE_GENERIC_TAG;
    adv_fields.appearance_is_present = 1;

    /* Set device LE role */
    adv_fields.le_role = BLE_GAP_LE_ROLE_PERIPHERAL;
    adv_fields.le_role_is_present = 1;

    /* Set advertiement fields */
    rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set advertising data, error code: %d", rc);
        return;
    }

    /* Set non-connetable and general discoverable mode to be a beacon */
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    /* Set advertising interval */
    adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(500);
    adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(510);
    uint8_t custom_mac[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
    ble_hs_id_set_rnd(custom_mac);

    /* Start advertising */
    rc = ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, NULL, BLE_HS_FOREVER, &adv_params,
                           advertiser_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to start advertising, error code: %d", rc);
        return;
    }
    ESP_LOGI(TAG, "advertising started!");
}

// Callback for connection events
static int central_event_callback(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                ESP_LOGI(TAG, "Connection established");
            } else {
                ESP_LOGE(TAG, "Connection failed");
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Disconnected");
            break;

        default:
            break;
    }
    return 0;
}

int scan_event_callback(struct ble_gap_event *event, void *arg) {
    if (event->type == BLE_GAP_EVENT_DISC) {
        ESP_LOGI(TAG, "Device found"); 
        if (event->disc.data[0]==14) {  //TODO: check for correct data
            ESP_LOGI(TAG, "Connecting to device...");
            ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &event->disc.addr,
                1000, NULL, central_event_callback, NULL);}
    }
    return 0;
}

/* Start scanning for advertisements */
void start_scanning(void) {
    struct ble_gap_disc_params disc_params;
    int rc;

    memset(&disc_params, 0, sizeof(disc_params));
    disc_params.passive = 1;
    disc_params.itvl = BLE_GAP_SCAN_SLOW_INTERVAL1;
    disc_params.window = BLE_GAP_SCAN_SLOW_WINDOW1;
    disc_params.filter_policy = BLE_HCI_SCAN_FILT_NO_WL;
    disc_params.limited = 0;

    rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, 200, &disc_params, scan_event_callback, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to start scanning; rc=%d", rc);
    } else {
        ESP_LOGI(TAG, "Scanning started");
    }
}


/*
 *  Stack event callback functions
 *      - on_stack_reset is called when host resets BLE stack due to errors
 *      - on_stack_sync is called when host has synced with controller
 */
static void on_stack_reset(int reason) {
    /* On reset, print reset reason to console */
    ESP_LOGI(TAG, "nimble stack reset! Reason: %d", reason);
}

static void on_stack_sync(void) {
    static uint8_t own_addr_type;
    int rc = 0;
    char addr_str[18] = {0};
    static uint8_t addr_val[6] = {0};

    /* Make sure we have proper BT identity address set */
    set_random_addr();
    rc = ble_hs_util_ensure_addr(1);
    if (rc != 0) {
        ESP_LOGE(TAG, "device does not have any available bt address!");
        return;
    }

    /* Figure out BT address to use while advertising */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to infer address type, error code: %d", rc);
        return;
    }

    /* Copy device address to addr_val */
    rc = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to copy device address, error code: %d", rc);
        return;
    }
    format_addr(addr_str, addr_val);
    ESP_LOGI(TAG, "device address now: %s", addr_str);
    start_scanning();
}

static void nimble_host_config_init(void) {
    /* Set host callbacks */
    ble_hs_cfg.reset_cb = on_stack_reset;
    ble_hs_cfg.sync_cb = on_stack_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /* Store host configuration */
    ble_store_config_init();
}

static void nimble_host_task(void *param) {
    /* Task entry log */
    ESP_LOGI(TAG, "nimble host task has been started!");

    /* This function won't return until nimble_port_stop() is executed */
    nimble_port_run();

    /* Clean up at exit */
    vTaskDelete(NULL);
}

void app_main(void) {
    /* Local variables */
    int rc;
    uint32_t seed = esp_random();
    esp_err_t ret;
    /* Random generator initialization */
    srand(seed);

    /*
     * NVS flash initialization
     * Dependency of BLE stack to store configurations
     */
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

    /* NimBLE stack initialization */
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize nimble stack, error code: %d ",
                 ret);
        return;
    }

    /* GAP service initialization */
    rc = gap_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to initialize GAP service, error code: %d", rc);
        return;
    }

    /* NimBLE host configuration initialization */
    nimble_host_config_init();

    /* Start NimBLE host task thread and return */
    xTaskCreate(nimble_host_task, "NimBLE Host", 4*1024, NULL, 5, NULL);
    return;
}
