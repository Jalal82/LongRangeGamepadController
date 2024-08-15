#include "esp_now_fcts.h"
#include "driver/uart.h"

// --------------------------- SBUS ------------------------------- //
#define NUM_CH 16
#define BUF_LEN_ 25
typedef struct
{
    bool lost_frame;
    bool failsafe;
    bool ch17, ch18;
    int16_t ch[NUM_CH];
} SbusData;

int32_t baud_ = 100000;
/* Message len */

/* SBUS message defs */
int8_t NUM_SBUS_CH_ = 16;
uint8_t HEADER_ = 0x0F;
uint8_t FOOTER_ = 0x00;
uint8_t FOOTER2_ = 0x04;
uint8_t CH17_MASK_ = 0x01;
uint8_t CH18_MASK_ = 0x02;
uint8_t LOST_FRAME_MASK_ = 0x04;
uint8_t FAILSAFE_MASK_ = 0x08;
/* Data */
uint8_t buf_[BUF_LEN_];
SbusData data_;
// --------------------------- SBUS ------------------------------- //
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

/*************************** */
#include <stdint.h>
#include <string.h>

int map_value(int input_value, int input_min, int input_max, int output_min, int output_max)
{
    int input_range = input_max - input_min;
    int output_range = output_max - output_min;

    // Scale the input value to the output range
    return (((input_value - input_min) * output_range / input_range) + output_min);
}

int get_channel(uint8_t byte1, uint8_t byte2)
{
    int value = ((byte2 << 8) | byte1);
    return map_value(value, 0, 65535, 172, 1811);
}

void Sbus_Write()
{
    // ESP_LOGI(TAG, "Received data  : " DATASTR ", len: %d", DATA2STR(send_data->hid_data), sizeof(send_data->hid_data));
    // uart_write_bytes(UART_NUM_2, data, strlen(data));

    data_.ch[0] = get_channel(send_data->hid_data[3], send_data->hid_data[4]);
    data_.ch[1] = get_channel(send_data->hid_data[5], send_data->hid_data[6]);
    data_.ch[2] = get_channel(send_data->hid_data[7], send_data->hid_data[8]);
    data_.ch[3] = get_channel(send_data->hid_data[9], send_data->hid_data[10]);
    printf("ch1: %d, ch2 : %d, ch3 : %d, ch4 : %d\n",data_.ch[0],data_.ch[1],data_.ch[3],data_.ch[4]);
    // /* Assemble packet */
    /* Assemble packet */
    buf_[0] = HEADER_;
    buf_[1] = (uint8_t)((data_.ch[0] & 0x07FF));
    buf_[2] = (uint8_t)((data_.ch[0] & 0x07FF) >> 8 |
                        (data_.ch[1] & 0x07FF) << 3);
    buf_[3] = (uint8_t)((data_.ch[1] & 0x07FF) >> 5 |
                        (data_.ch[2] & 0x07FF) << 6);
    buf_[4] = (uint8_t)((data_.ch[2] & 0x07FF) >> 2);
    buf_[5] = (uint8_t)((data_.ch[2] & 0x07FF) >> 10 |
                        (data_.ch[3] & 0x07FF) << 1);
    buf_[6] = (uint8_t)((data_.ch[3] & 0x07FF) >> 7 |
                        (data_.ch[4] & 0x07FF) << 4);
    buf_[7] = (uint8_t)((data_.ch[4] & 0x07FF) >> 4 |
                        (data_.ch[5] & 0x07FF) << 7);
    buf_[8] = (uint8_t)((data_.ch[5] & 0x07FF) >> 1);
    buf_[9] = (uint8_t)((data_.ch[5] & 0x07FF) >> 9 |
                        (data_.ch[6] & 0x07FF) << 2);
    buf_[10] = (uint8_t)((data_.ch[6] & 0x07FF) >> 6 |
                         (data_.ch[7] & 0x07FF) << 5);
    buf_[11] = (uint8_t)((data_.ch[7] & 0x07FF) >> 3);
    buf_[12] = (uint8_t)((data_.ch[8] & 0x07FF));
    buf_[13] = (uint8_t)((data_.ch[8] & 0x07FF) >> 8 |
                         (data_.ch[9] & 0x07FF) << 3);
    buf_[14] = (uint8_t)((data_.ch[9] & 0x07FF) >> 5 |
                         (data_.ch[10] & 0x07FF) << 6);
    buf_[15] = (uint8_t)((data_.ch[10] & 0x07FF) >> 2);
    buf_[16] = (uint8_t)((data_.ch[10] & 0x07FF) >> 10 |
                         (data_.ch[11] & 0x07FF) << 1);
    buf_[17] = (uint8_t)((data_.ch[11] & 0x07FF) >> 7 |
                         (data_.ch[12] & 0x07FF) << 4);
    buf_[18] = (uint8_t)((data_.ch[12] & 0x07FF) >> 4 |
                         (data_.ch[13] & 0x07FF) << 7);
    buf_[19] = (uint8_t)((data_.ch[13] & 0x07FF) >> 1);
    buf_[20] = (uint8_t)((data_.ch[13] & 0x07FF) >> 9 |
                         (data_.ch[14] & 0x07FF) << 2);
    buf_[21] = (uint8_t)((data_.ch[14] & 0x07FF) >> 6 |
                         (data_.ch[15] & 0x07FF) << 5);
    buf_[22] = (uint8_t)((data_.ch[15] & 0x07FF) >> 3);
    buf_[23] = 0x00 | (data_.ch17 * CH17_MASK_) | (data_.ch18 * CH18_MASK_) |
               (data_.failsafe * FAILSAFE_MASK_) |
               (data_.lost_frame * LOST_FRAME_MASK_);
    buf_[24] = FOOTER_;
    uart_write_bytes(UART_NUM_2, buf_, sizeof(buf_));
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
            ESP_LOGE(TAG, "Callback type error: %d", evt.id);
            break;
        case EXAMPLE_ESPNOW_RECV_CB:
            espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;
            memcpy(send_data->hid_data, recv_cb->data, recv_cb->data_len);
            // ESP_LOGI(TAG, "Received data  : " DATASTR " from: " MACSTR ", len: %d", DATA2STR(recv_cb->data), MAC2STR(recv_cb->mac_addr), recv_cb->data_len);
            Sbus_Write();
            break;
        default:
            ESP_LOGE(TAG, "Callback type error: %d", evt.id);
            break;
        }
    }
}

esp_err_t Espnow_init(void)
{

    uart_config_t uart_config = {
        .baud_rate = 100000,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_EVEN,
        .stop_bits = UART_STOP_BITS_2,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};

    uart_param_config(UART_NUM_2, &uart_config);
    uart_set_pin(UART_NUM_2, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_set_line_inverse(UART_NUM_2, UART_SIGNAL_TXD_INV);

    uart_driver_install(UART_NUM_2, 1024, 0, 0, NULL, 0);

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
