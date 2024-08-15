#include "esp_now_fcts.h"
#include "bl_fcts.h"

Gampad_Data *data;

/*------------------------------------ BT Configuration and Vars ----------------------------------------*/
#define SCAN_DURATION_SECONDS 4
static const char *BT_TAG = "BT_TASK";
void bt_hid_task(void *pvParameters);
void hidh_callback(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    esp_hidh_event_t event = (esp_hidh_event_t)id;
    esp_hidh_event_data_t *param = (esp_hidh_event_data_t *)event_data;

    switch (event)
    {
    case ESP_HIDH_OPEN_EVENT:
    {
        if (param->open.status == ESP_OK)
        {
            const uint8_t *bda = esp_hidh_dev_bda_get(param->open.dev);
            ESP_LOGI(BT_TAG, ESP_BD_ADDR_STR " OPEN: %s", ESP_BD_ADDR_HEX(bda), esp_hidh_dev_name_get(param->open.dev));
            esp_hidh_dev_dump(param->open.dev, stdout);
        }
        else
        {
            ESP_LOGE(BT_TAG, " OPEN failed!");
        }
        break;
    }
    case ESP_HIDH_BATTERY_EVENT:
    {
        const uint8_t *bda = esp_hidh_dev_bda_get(param->battery.dev);
        ESP_LOGI(BT_TAG, ESP_BD_ADDR_STR " BATTERY: %d%%", ESP_BD_ADDR_HEX(bda), param->battery.level);
        break;
    }
    case ESP_HIDH_INPUT_EVENT:
    {

#if DEBUG_INPUT_ALL
        const uint8_t *bda = esp_hidh_dev_bda_get(param->input.dev);
        ESP_LOGI(BT_TAG, ESP_BD_ADDR_STR " INPUT: %8s, MAP: %2u, ID: %3u, Len: %d, Data:", ESP_BD_ADDR_HEX(bda), esp_hid_usage_str(param->input.usage), param->input.map_index, param->input.report_id, param->input.length);
        ESP_LOG_BUFFER_HEX(BT_TAG, param->input.data, param->input.length);
#elif DEBUG_INPUT

        ESP_LOG_BUFFER_HEX(BT_TAG, param->input.data, param->input.length);
#endif
    memcpy(data->hid_data,param->input.data,param->input.length);
    esp_now_send_data(data);
        break;
    }
    case ESP_HIDH_FEATURE_EVENT:
    {
        const uint8_t *bda = esp_hidh_dev_bda_get(param->feature.dev);
        ESP_LOGI(BT_TAG, ESP_BD_ADDR_STR " FEATURE: %8s, MAP: %2u, ID: %3u, Len: %d", ESP_BD_ADDR_HEX(bda),
                 esp_hid_usage_str(param->feature.usage), param->feature.map_index, param->feature.report_id,
                 param->feature.length);
        ESP_LOG_BUFFER_HEX(BT_TAG, param->feature.data, param->feature.length);
        break;
    }
    case ESP_HIDH_CLOSE_EVENT:
    {
        const uint8_t *bda = esp_hidh_dev_bda_get(param->close.dev);
        ESP_LOGI(BT_TAG, ESP_BD_ADDR_STR " CLOSE: %s", ESP_BD_ADDR_HEX(bda), esp_hidh_dev_name_get(param->close.dev));
        ESP_LOGI(BT_TAG, "Restarting BT Task");
        clear_saved_devices();
        xTaskCreate(&bt_hid_task, "hid_task", 6 * 1024, NULL, 2, NULL);
        break;
    }
    default:
        ESP_LOGI(BT_TAG, "EVENT: %d", event);
        break;
    }
}

void bt_hid_task(void *pvParameters)
{
    size_t results_len = 0;
    esp_hid_scan_result_t *results = NULL;
    while (results_len == 0)
    {
        ESP_LOGI(BT_TAG, "SCAN...");
        // start scan for HID devices
        esp_hid_scan(SCAN_DURATION_SECONDS, &results_len, &results);
        ESP_LOGI(BT_TAG, "SCAN: %u results", results_len);
    }

    if (results_len)
    {
        esp_hid_scan_result_t *r = results;
        esp_hid_scan_result_t *cr = NULL;
        while (r)
        {
            printf("  %s: " ESP_BD_ADDR_STR ", ", (r->transport == ESP_HID_TRANSPORT_BLE) ? "BLE" : "BT ", ESP_BD_ADDR_HEX(r->bda));
            printf("RSSI: %d, ", r->rssi);
            printf("USAGE: %s, ", esp_hid_usage_str(r->usage));

#if CONFIG_BT_HID_HOST_ENABLED
            if (r->transport == ESP_HID_TRANSPORT_BT)
            {
                cr = r;
                printf("COD: %s[", esp_hid_cod_major_str(r->bt.cod.major));
                esp_hid_cod_minor_print(r->bt.cod.minor, stdout);
                printf("] srv 0x%03x, ", r->bt.cod.service);
                print_uuid(&r->bt.uuid);
                printf(", ");
            }
#endif /* CONFIG_BT_HID_HOST_ENABLED */
            printf("NAME: %s ", r->name ? r->name : "");
            printf("\n");

            if (r->bda[0] == 0x98 && r->bda[1] == 0xB6 && r->bda[2] == 0xE9 && r->bda[3] == 0xB7 && r->bda[4] == 0x11 && r->bda[5] == 0x11)
            {
                cr = r;
                break;
            }
            r = r->next;
        }
        if (cr)
        {
            printf("gamepad detected");
            // open the last result
            esp_hidh_dev_open(cr->bda, cr->transport, cr->ble.addr_type);
        }
        else
        {
            printf("Error: Device not found\n");
        }
        // free the results
        esp_hid_scan_results_free(results);
    }
    vTaskDelete(NULL);
}
esp_hidh_config_t Bt_Config = {
    .callback = hidh_callback,
    .event_stack_size = 4096,
    .callback_arg = NULL,
};

void app_main()
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    Wifi_init();
    Espnow_init();

    ESP_LOGI(BT_TAG, "setting hid gap, mode:%d", HID_HOST_MODE);
    ESP_ERROR_CHECK(esp_hid_gap_init(HID_HOST_MODE));
    ESP_ERROR_CHECK(esp_hidh_init(&Bt_Config));
    clear_saved_devices();
    xTaskCreate(&bt_hid_task, "hid_task", 6 * 1024, NULL, 2, NULL);

    data = malloc(sizeof(Gampad_Data));
}
