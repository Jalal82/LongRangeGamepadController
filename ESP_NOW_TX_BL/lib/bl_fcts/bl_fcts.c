#include "bl_fcts.h"


#include <string.h>
#include <stdbool.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "BT";

#define GAP_DBG_PRINTF(...) printf(__VA_ARGS__)
static const char *gap_bt_prop_type_names[5] = {"", "BDNAME", "COD", "RSSI", "EIR"};

#define SIZEOF_ARRAY(a) (sizeof(a) / sizeof(*a))

static const char *bt_gap_evt_names[] = {"DISC_RES", "DISC_STATE_CHANGED", "RMT_SRVCS", "RMT_SRVC_REC", "AUTH_CMPL", "PIN_REQ", "CFM_REQ", "KEY_NOTIF", "KEY_REQ", "READ_RSSI_DELTA"};

static esp_hid_scan_result_t *bt_scan_results = NULL;
static size_t num_bt_scan_results = 0;

static SemaphoreHandle_t bt_hidh_cb_semaphore = NULL;
#define WAIT_BT_CB() xSemaphoreTake(bt_hidh_cb_semaphore, portMAX_DELAY)
#define SEND_BT_CB() xSemaphoreGive(bt_hidh_cb_semaphore)

void print_uuid(esp_bt_uuid_t *uuid)
{
    if (uuid->len == ESP_UUID_LEN_16)
    {
        GAP_DBG_PRINTF("UUID16: 0x%04x", uuid->uuid.uuid16);
    }
    else if (uuid->len == ESP_UUID_LEN_32)
    {
        GAP_DBG_PRINTF("UUID32: 0x%08" PRIx32, uuid->uuid.uuid32);
    }
    else if (uuid->len == ESP_UUID_LEN_128)
    {
        GAP_DBG_PRINTF("UUID128: %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x", uuid->uuid.uuid128[0],
                       uuid->uuid.uuid128[1], uuid->uuid.uuid128[2], uuid->uuid.uuid128[3],
                       uuid->uuid.uuid128[4], uuid->uuid.uuid128[5], uuid->uuid.uuid128[6],
                       uuid->uuid.uuid128[7], uuid->uuid.uuid128[8], uuid->uuid.uuid128[9],
                       uuid->uuid.uuid128[10], uuid->uuid.uuid128[11], uuid->uuid.uuid128[12],
                       uuid->uuid.uuid128[13], uuid->uuid.uuid128[14], uuid->uuid.uuid128[15]);
    }
}

const char *bt_gap_evt_str(uint8_t event)
{
    if (event >= SIZEOF_ARRAY(bt_gap_evt_names))
    {
        return "UNKNOWN";
    }
    return bt_gap_evt_names[event];
}

void esp_hid_scan_results_free(esp_hid_scan_result_t *results)
{
    esp_hid_scan_result_t *r = NULL;
    while (results)
    {
        r = results;
        results = results->next;
        if (r->name != NULL)
        {
            free((char *)r->name);
        }
        free(r);
    }
}

static esp_hid_scan_result_t *find_scan_result(esp_bd_addr_t bda, esp_hid_scan_result_t *results)
{
    esp_hid_scan_result_t *r = results;
    while (r)
    {
        if (memcmp(bda, r->bda, sizeof(esp_bd_addr_t)) == 0)
        {
            return r;
        }
        r = r->next;
    }
    return NULL;
}

static void add_bt_scan_result(esp_bd_addr_t bda, esp_bt_cod_t *cod, esp_bt_uuid_t *uuid, uint8_t *name, uint8_t name_len, int rssi)
{
    esp_hid_scan_result_t *r = find_scan_result(bda, bt_scan_results);
    if (r)
    {
        // Some info may come later
        if (r->name == NULL && name && name_len)
        {
            char *name_s = (char *)malloc(name_len + 1);
            if (name_s == NULL)
            {
                ESP_LOGE(TAG, "Malloc result name failed!");
                return;
            }
            memcpy(name_s, name, name_len);
            name_s[name_len] = 0;
            r->name = (const char *)name_s;
        }
        if (r->bt.uuid.len == 0 && uuid->len)
        {
            memcpy(&r->bt.uuid, uuid, sizeof(esp_bt_uuid_t));
        }
        if (rssi != 0)
        {
            r->rssi = rssi;
        }
        return;
    }

    r = (esp_hid_scan_result_t *)malloc(sizeof(esp_hid_scan_result_t));
    if (r == NULL)
    {
        ESP_LOGE(TAG, "Malloc bt_hidh_scan_result_t failed!");
        return;
    }
    r->transport = ESP_HID_TRANSPORT_BT;
    memcpy(r->bda, bda, sizeof(esp_bd_addr_t));
    memcpy(&r->bt.cod, cod, sizeof(esp_bt_cod_t));
    memcpy(&r->bt.uuid, uuid, sizeof(esp_bt_uuid_t));
    r->usage = esp_hid_usage_from_cod((uint32_t)cod);
    r->rssi = rssi;
    r->name = NULL;
    if (name_len && name)
    {
        char *name_s = (char *)malloc(name_len + 1);
        if (name_s == NULL)
        {
            free(r);
            ESP_LOGE(TAG, "Malloc result name failed!");
            return;
        }
        memcpy(name_s, name, name_len);
        name_s[name_len] = 0;
        r->name = (const char *)name_s;
    }
    r->next = bt_scan_results;
    bt_scan_results = r;
    num_bt_scan_results++;
}

static void handle_bt_device_result(struct disc_res_param *disc_res)
{
    GAP_DBG_PRINTF("BT : " ESP_BD_ADDR_STR, ESP_BD_ADDR_HEX(disc_res->bda));
    uint32_t codv = 0;
    esp_bt_cod_t *cod = (esp_bt_cod_t *)&codv;
    int8_t rssi = 0;
    uint8_t *name = NULL;
    uint8_t name_len = 0;
    esp_bt_uuid_t uuid;

    uuid.len = ESP_UUID_LEN_16;
    uuid.uuid.uuid16 = 0;

    for (int i = 0; i < disc_res->num_prop; i++)
    {
        esp_bt_gap_dev_prop_t *prop = &disc_res->prop[i];
        if (prop->type != ESP_BT_GAP_DEV_PROP_EIR)
        {
            GAP_DBG_PRINTF(", %s: ", gap_bt_prop_type_names[prop->type]);
        }
        if (prop->type == ESP_BT_GAP_DEV_PROP_BDNAME)
        {
            name = (uint8_t *)prop->val;
            name_len = strlen((const char *)name);
            GAP_DBG_PRINTF("%s", (const char *)name);
        }
        else if (prop->type == ESP_BT_GAP_DEV_PROP_RSSI)
        {
            rssi = *((int8_t *)prop->val);
            GAP_DBG_PRINTF("%d", rssi);
        }
        else if (prop->type == ESP_BT_GAP_DEV_PROP_COD)
        {
            memcpy(&codv, prop->val, sizeof(uint32_t));
            GAP_DBG_PRINTF("major: %s, minor: %d, service: 0x%03x", esp_hid_cod_major_str(cod->major), cod->minor, cod->service);
        }
        else if (prop->type == ESP_BT_GAP_DEV_PROP_EIR)
        {
            uint8_t len = 0;
            uint8_t *data = 0;

            data = esp_bt_gap_resolve_eir_data((uint8_t *)prop->val, ESP_BT_EIR_TYPE_CMPL_16BITS_UUID, &len);
            if (data == NULL)
            {
                data = esp_bt_gap_resolve_eir_data((uint8_t *)prop->val, ESP_BT_EIR_TYPE_INCMPL_16BITS_UUID, &len);
            }
            if (data && len == ESP_UUID_LEN_16)
            {
                uuid.len = ESP_UUID_LEN_16;
                uuid.uuid.uuid16 = data[0] + (data[1] << 8);
                GAP_DBG_PRINTF(", ");
                print_uuid(&uuid);
                continue;
            }

            data = esp_bt_gap_resolve_eir_data((uint8_t *)prop->val, ESP_BT_EIR_TYPE_CMPL_32BITS_UUID, &len);
            if (data == NULL)
            {
                data = esp_bt_gap_resolve_eir_data((uint8_t *)prop->val, ESP_BT_EIR_TYPE_INCMPL_32BITS_UUID, &len);
            }
            if (data && len == ESP_UUID_LEN_32)
            {
                uuid.len = len;
                memcpy(&uuid.uuid.uuid32, data, sizeof(uint32_t));
                GAP_DBG_PRINTF(", ");
                print_uuid(&uuid);
                continue;
            }

            data = esp_bt_gap_resolve_eir_data((uint8_t *)prop->val, ESP_BT_EIR_TYPE_CMPL_128BITS_UUID, &len);
            if (data == NULL)
            {
                data = esp_bt_gap_resolve_eir_data((uint8_t *)prop->val, ESP_BT_EIR_TYPE_INCMPL_128BITS_UUID, &len);
            }
            if (data && len == ESP_UUID_LEN_128)
            {
                uuid.len = len;
                memcpy(uuid.uuid.uuid128, (uint8_t *)data, len);
                GAP_DBG_PRINTF(", ");
                print_uuid(&uuid);
                continue;
            }

            // try to find a name
            if (name == NULL)
            {
                data = esp_bt_gap_resolve_eir_data((uint8_t *)prop->val, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &len);
                if (data == NULL)
                {
                    data = esp_bt_gap_resolve_eir_data((uint8_t *)prop->val, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &len);
                }
                if (data && len)
                {
                    name = data;
                    name_len = len;
                    GAP_DBG_PRINTF(", NAME: ");
                    for (int x = 0; x < len; x++)
                    {
                        GAP_DBG_PRINTF("%c", (char)data[x]);
                    }
                }
            }
        }
    }
    GAP_DBG_PRINTF("\n");

    if (cod->major == ESP_BT_COD_MAJOR_DEV_PERIPHERAL || (find_scan_result(disc_res->bda, bt_scan_results) != NULL))
    {
        add_bt_scan_result(disc_res->bda, cod, &uuid, name, name_len, rssi);
    }
}

static void bt_gap_event_handler(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event)
    {
    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
    {
        ESP_LOGV(TAG, "BT GAP DISC_STATE %s", (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED) ? "START" : "STOP");
        if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED)
        {
            SEND_BT_CB();
        }
        break;
    }
    case ESP_BT_GAP_DISC_RES_EVT:
    {
        handle_bt_device_result(&param->disc_res);
        break;
    }
#if (CONFIG_BT_SSP_ENABLED)
    case ESP_BT_GAP_KEY_NOTIF_EVT:
        ESP_LOGI(TAG, "BT GAP KEY_NOTIF passkey:%" PRIu32, param->key_notif.passkey);
        break;
    case ESP_BT_GAP_CFM_REQ_EVT:
    {
        ESP_LOGI(TAG, "ESP_BT_GAP_CFM_REQ_EVT Please compare the numeric value: %" PRIu32, param->cfm_req.num_val);
        esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
        break;
    }
#endif
    case ESP_BT_GAP_MODE_CHG_EVT:
        ESP_LOGI(TAG, "BT GAP MODE_CHG_EVT mode:%d", param->mode_chg.mode);
        break;
    case ESP_BT_GAP_PIN_REQ_EVT:
    {
        ESP_LOGI(TAG, "ESP_BT_GAP_PIN_REQ_EVT min_16_digit:%d", param->pin_req.min_16_digit);
        if (param->pin_req.min_16_digit)
        {
            ESP_LOGI(TAG, "Input pin code: 0000 0000 0000 0000");
            esp_bt_pin_code_t pin_code = {0};
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 16, pin_code);
        }
        else
        {
            ESP_LOGI(TAG, "Input pin code: 1234");
            esp_bt_pin_code_t pin_code;
            pin_code[0] = '1';
            pin_code[1] = '2';
            pin_code[2] = '3';
            pin_code[3] = '4';
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin_code);
        }
        break;
    }
    default:
        ESP_LOGW(TAG, "BT GAP EVENT %s", bt_gap_evt_str(event));
        break;
    }
}

 esp_err_t init_bt_gap(void)
{
    esp_err_t ret;
#if (CONFIG_BT_SSP_ENABLED)
    /* Set default parameters for Secure Simple Pairing */
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_IO;
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));
#endif
    /*
     * Set default parameters for Legacy Pairing
     * Use variable pin, input pin code when pairing
     */
    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_VARIABLE;
    esp_bt_pin_code_t pin_code;
    esp_bt_gap_set_pin(pin_type, 0, pin_code);

    if ((ret = esp_bt_gap_register_callback(bt_gap_event_handler)) != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_bt_gap_register_callback failed: %d", ret);
        return ret;
    }

    // Allow BT devices to connect back to us
    if ((ret = esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_NON_DISCOVERABLE)) != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_bt_gap_set_scan_mode failed: %d", ret);
        return ret;
    }
    return ret;
}

static esp_err_t start_bt_scan(uint32_t seconds)
{
    esp_err_t ret = ESP_OK;
    if ((ret = esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, (int)(seconds / 1.28), 0)) != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_bt_gap_start_discovery failed: %d", ret);
        return ret;
    }
    return ret;
}

static esp_err_t init_low_level(uint8_t mode)
{
    esp_err_t ret;
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
#if CONFIG_IDF_TARGET_ESP32
    bt_cfg.mode = mode;
#endif
#if CONFIG_BT_HID_HOST_ENABLED
    if (mode & ESP_BT_MODE_CLASSIC_BT)
    {
        bt_cfg.bt_max_acl_conn = 3;
        bt_cfg.bt_max_sync_conn = 3;
    }
    else
#endif
    {
        ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
        if (ret)
        {
            ESP_LOGE(TAG, "esp_bt_controller_mem_release failed: %d", ret);
            return ret;
        }
    }
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret)
    {
        ESP_LOGE(TAG, "esp_bt_controller_init failed: %d", ret);
        return ret;
    }

    ret = esp_bt_controller_enable(mode);
    if (ret)
    {
        ESP_LOGE(TAG, "esp_bt_controller_enable failed: %d", ret);
        return ret;
    }

    ret = esp_bluedroid_init();
    if (ret)
    {
        ESP_LOGE(TAG, "esp_bluedroid_init failed: %d", ret);
        return ret;
    }

    ret = esp_bluedroid_enable();
    if (ret)
    {
        ESP_LOGE(TAG, "esp_bluedroid_enable failed: %d", ret);
        return ret;
    }
#if CONFIG_BT_HID_HOST_ENABLED
    if (mode & ESP_BT_MODE_CLASSIC_BT)
    {
        ret = init_bt_gap();
        if (ret)
        {
            return ret;
        }
    }
#endif
#if CONFIG_BT_BLE_ENABLED
    if (mode & ESP_BT_MODE_BLE)
    {
        ret = init_ble_gap();
        if (ret)
        {
            return ret;
        }
    }
#endif /* CONFIG_BT_BLE_ENABLED */
    return ret;
}
#include "esp_bt.h"
#include "esp_log.h"

esp_err_t clear_saved_devices()
{
    esp_err_t ret;
    int dev_num = esp_bt_gap_get_bond_device_num();
    if (dev_num > 0)
    {
        esp_bd_addr_t *dev_list = (esp_bd_addr_t *)malloc(dev_num * sizeof(esp_bd_addr_t));
        if (dev_list == NULL)
        {
            ESP_LOGE(TAG, "Failed to allocate memory for device list");
            return ESP_ERR_NO_MEM;
        }

        ret = esp_bt_gap_get_bond_device_list(&dev_num, dev_list);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to get bonded device list");
            free(dev_list);
            return ret;
        }

        for (int i = 0; i < dev_num; i++)
        {
            ESP_LOGI(TAG, "Deleting bonded device with address: %02x:%02x:%02x:%02x:%02x:%02x",
                     dev_list[i][0], dev_list[i][1], dev_list[i][2],
                     dev_list[i][3], dev_list[i][4], dev_list[i][5]);
            ret = esp_bt_gap_remove_bond_device(dev_list[i]);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to remove bonded device");
            }
        }

        free(dev_list);
        ESP_LOGI(TAG, "Cleared saved Bluetooth devices");
    }
    else
    {
        ESP_LOGI(TAG, "No Device data found in nvs memory proceed to main Code!");
    }

    return ESP_OK;
}

esp_err_t esp_hid_gap_init(uint8_t mode)
{
    esp_err_t ret;
    if (!mode || mode > ESP_BT_MODE_BTDM)
    {
        ESP_LOGE(TAG, "Invalid mode given!");
        return ESP_FAIL;
    }

    if (bt_hidh_cb_semaphore != NULL)
    {
        ESP_LOGE(TAG, "Already initialised");
        return ESP_FAIL;
    }

    bt_hidh_cb_semaphore = xSemaphoreCreateBinary();
    if (bt_hidh_cb_semaphore == NULL)
    {
        ESP_LOGE(TAG, "xSemaphoreCreateMutex failed!");
        return ESP_FAIL;
    }

    ret = init_low_level(mode);
    if (ret != ESP_OK)
    {
        vSemaphoreDelete(bt_hidh_cb_semaphore);
        bt_hidh_cb_semaphore = NULL;
        return ret;
    }

    return ESP_OK;
}

esp_err_t esp_hid_scan(uint32_t seconds, size_t *num_results, esp_hid_scan_result_t **results)
{
    if (num_bt_scan_results || bt_scan_results)
    {
        ESP_LOGE(TAG, "There are old scan results. Free them first!");
        return ESP_FAIL;
    }

#if CONFIG_BT_BLE_ENABLED
    if (start_ble_scan(seconds) == ESP_OK)
    {
        WAIT_BLE_CB();
    }
    else
    {
        return ESP_FAIL;
    }
#endif /* CONFIG_BT_BLE_ENABLED */

#if CONFIG_BT_HID_HOST_ENABLED
    if (start_bt_scan(seconds) == ESP_OK)
    {
        WAIT_BT_CB();
    }
    else
    {
        return ESP_FAIL;
    }
#endif

    *num_results = num_bt_scan_results;
    *results = bt_scan_results;
    if (num_bt_scan_results)
    {
        while (bt_scan_results->next != NULL)
        {
            bt_scan_results = bt_scan_results->next;
        }
    }

    num_bt_scan_results = 0;
    bt_scan_results = NULL;
    return ESP_OK;
}
