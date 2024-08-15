

#ifndef _ESP_NOW_FCT_H_
#define _ESP_NOW_FCT_H_

#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"

#include "nvs_flash.h"
#include "esp_random.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_crc.h"

#include "driver/gpio.h"

#define TXD_PIN (17)
#define RXD_PIN (16)

#ifdef __cplusplus
extern "C"
{
#endif


/* ESPNOW can work in both station and softap mode. It is configured in menuconfig. */
#if CONFIG_ESPNOW_WIFI_MODE_STATION
#define ESPNOW_WIFI_MODE WIFI_MODE_STA
#define ESPNOW_WIFI_IF ESP_IF_WIFI_STA
#else
#define ESPNOW_WIFI_MODE WIFI_MODE_AP
#define ESPNOW_WIFI_IF ESP_IF_WIFI_AP
#endif

#define TARGET_TX_RX 0 // 1 for TX and 0 for RX
#define BLINK_GPIO 2

#define ESPNOW_MAXDELAY 512
#define ESPNOW_QUEUE_SIZE 6
#define DATA2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5], (a)[6], (a)[7], (a)[8], (a)[9], (a)[10]
#define DATASTR "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x"

    typedef struct
    {
        uint8_t hid_data[11];
        uint8_t last_update;
    } Gampad_Data;

    typedef enum
    {
        EXAMPLE_ESPNOW_SEND_CB,
        EXAMPLE_ESPNOW_RECV_CB,
    } espnow_event_id_t;

    typedef struct
    {
        uint8_t mac_addr[ESP_NOW_ETH_ALEN];
        esp_now_send_status_t status;
    } espnow_event_send_cb_t;

    typedef struct
    {
        uint8_t mac_addr[ESP_NOW_ETH_ALEN];
        uint8_t *data;
        int data_len;
    } espnow_event_recv_cb_t;

    typedef union
    {
        espnow_event_send_cb_t send_cb;
        espnow_event_recv_cb_t recv_cb;
    } espnow_event_info_t;

    /* When ESPNOW sending or receiving callback function is called, post event to ESPNOW task. */
    typedef struct
    {
        espnow_event_id_t id;
        espnow_event_info_t info;
    } espnow_event_t;
    extern Gampad_Data *send_data;
    void Wifi_init();
    esp_err_t Espnow_init(void);
    void espnow_deinit(Gampad_Data *send_data);
    void Espnow_task(void *pvParameter);
    esp_err_t esp_now_send_data(Gampad_Data *data);

#ifdef __cplusplus
}
#endif

#endif