#include "esp_now_fcts.h"

Gampad_Data *send_data;
const char *TAG = "ESP_NOW";
#if TARGET_TX_RX
static uint8_t target_mac[ESP_NOW_ETH_ALEN] = {0x30, 0xc6, 0xf7, 0x23, 0x28, 0x11};
#else
static uint8_t sender_mac[ESP_NOW_ETH_ALEN] = {0x0c, 0xdc, 0x7e, 0x62, 0xb9, 0xa9};
#endif
static QueueHandle_t s_espnow_queue;

/* WiFi should start before using ESPNOW */
void Wifi_init()
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(ESPNOW_WIFI_MODE));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(CONFIG_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

#if CONFIG_ESPNOW_ENABLE_LONG_RANGE
    ESP_ERROR_CHECK(esp_wifi_set_protocol(ESPNOW_WIFI_IF, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR));
#endif
    esp_rom_gpio_pad_select_gpio(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

#if TARGET_TX_RX
/* ESPNOW sending or receiving callback function is called in WiFi task.
 * Users should not do lengthy operations from this task. Instead, post
 * necessary data to a queue and handle it from a lower priority task. */
static void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{

    if (status != ESP_OK)
    {
        ESP_LOGE(TAG, "Sent Error");
        return;
    }
}
#else
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    espnow_event_t evt;
    espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;
    uint8_t *mac_addr = recv_info->src_addr;

    if (mac_addr == NULL || data == NULL || len <= 0)
    {
        ESP_LOGE(TAG, "Receive cb arg error");
        return;
    }

    evt.id = EXAMPLE_ESPNOW_RECV_CB;
    memcpy(recv_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    recv_cb->data = malloc(len);
    if (recv_cb->data == NULL)
    {
        ESP_LOGE(TAG, "Malloc receive data fail");
        return;
    }
    memcpy(recv_cb->data, data, len);
    recv_cb->data_len = len;

    if (xQueueSend(s_espnow_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE)
    {
        ESP_LOGW(TAG, "Send receive queue fail");
        free(recv_cb->data);
    }
}
#endif

void espnow_deinit(Gampad_Data *send_data)
{
    free(send_data);
    vSemaphoreDelete(s_espnow_queue);
    esp_now_deinit();
}

void Espnow_task(void *pvParameter)
{
    espnow_event_t evt;

    vTaskDelay(2000 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Starting espnow task");
    
    gpio_set_level(BLINK_GPIO, 1);
    vTaskDelay(800 / portTICK_PERIOD_MS);
    gpio_set_level(BLINK_GPIO, 0);

    while (xQueueReceive(s_espnow_queue, &evt, portMAX_DELAY) == pdTRUE)
    {
        switch (evt.id)
        {
        case EXAMPLE_ESPNOW_SEND_CB:

            break;
        case EXAMPLE_ESPNOW_RECV_CB:
            espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;
            memcpy(send_data->hid_data,recv_cb->data,recv_cb->data_len);
            ESP_LOGI(TAG, "Received data  : " DATASTR " from: " MACSTR ", len: %d", DATA2STR(recv_cb->data), MAC2STR(recv_cb->mac_addr), recv_cb->data_len);
            break;
        default:
            ESP_LOGE(TAG, "Callback type error: %d", evt.id);
            break;
        }
    }
}

esp_err_t Espnow_init(void)
{

    s_espnow_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(espnow_event_t));
    if (s_espnow_queue == NULL)
    {
        ESP_LOGE(TAG, "Create mutex fail");
        return ESP_FAIL;
    }

    /* Initialize ESPNOW and register sending and receiving callback function. */
    ESP_ERROR_CHECK(esp_now_init());
#if TARGET_TX_RX
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
#else
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
#endif

    /* Set primary master key. */
    ESP_ERROR_CHECK(esp_now_set_pmk((uint8_t *)CONFIG_ESPNOW_PMK));

    /* Add broadcast peer information to peer list. */
    esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL)
    {
        ESP_LOGE(TAG, "Malloc peer information fail");
        vSemaphoreDelete(s_espnow_queue);
        esp_now_deinit();
        return ESP_FAIL;
    }
    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = CONFIG_ESPNOW_CHANNEL;
    peer->ifidx = ESPNOW_WIFI_IF;
    peer->encrypt = false;
#if TARGET_TX_RX
    memcpy(peer->peer_addr, target_mac, ESP_NOW_ETH_ALEN);
#else
    memcpy(peer->peer_addr, sender_mac, ESP_NOW_ETH_ALEN);
#endif
    ESP_ERROR_CHECK(esp_now_add_peer(peer));
    free(peer);
    ESP_LOGI(TAG, "Espnow init done");

    send_data = malloc(sizeof(Gampad_Data));

    if (send_data == NULL)
    {
        ESP_LOGE(TAG, "Malloc send parameter fail");
        vSemaphoreDelete(s_espnow_queue);
        esp_now_deinit();
        return ESP_FAIL;
    }
    memset(send_data, 0, sizeof(Gampad_Data));
    xTaskCreate(Espnow_task, "Espnow_task", 2048, send_data, 4, NULL);
    return ESP_OK;
}

esp_err_t esp_now_send_data(Gampad_Data *data)
{
#if TARGET_TX_RX
    ESP_LOGI(TAG, "send data : " DATASTR " to : " MACSTR "", DATA2STR(data->hid_data), MAC2STR(target_mac));
    /* Send the next data after the previous data is sent. */
    if (esp_now_send(target_mac, data->hid_data, 11) != ESP_OK)
    {
        ESP_LOGE(TAG, "Send error");
    }
#endif

    return ESP_OK;
}