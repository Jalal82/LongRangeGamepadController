#ifndef _BL_FCTS_H_
#define _BL_FCTS_H_

#include "esp_log.h"
#include "esp_err.h"
#include "stdio.h"

#define DEBUG_INPUT_ALL 0
#define DEBUG_INPUT     1

#define HIDH_IDLE_MODE 0x00
#define HIDH_BLE_MODE 0x01
#define HIDH_BT_MODE 0x02
#define HIDH_BTDM_MODE 0x03

#if CONFIG_BT_HID_HOST_ENABLED
#if CONFIG_BT_BLE_ENABLED
#define HID_HOST_MODE HIDH_BTDM_MODE
#else
#define HID_HOST_MODE HIDH_BT_MODE
#endif
#elif CONFIG_BT_BLE_ENABLED
#define HID_HOST_MODE HIDH_BLE_MODE
#else
#define HID_HOST_MODE HIDH_IDLE_MODE
#endif


#include "esp_bt.h"

#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_hid_common.h"


#include "esp_hidh.h"

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


#ifdef __cplusplus
extern "C" {
#endif

typedef struct esp_hidh_scan_result_s {
    struct esp_hidh_scan_result_s *next;

    esp_bd_addr_t bda;
    const char *name;
    int8_t rssi;
    esp_hid_usage_t usage;
    esp_hid_transport_t transport; //BT, BLE or USB
    union {
        struct {
            esp_bt_cod_t cod;
            esp_bt_uuid_t uuid;
        } bt;
        struct {
            esp_ble_addr_type_t addr_type;
            uint16_t appearance;
        } ble;
    };
} esp_hid_scan_result_t;

esp_err_t esp_hid_gap_init(uint8_t mode);
esp_err_t esp_hid_scan(uint32_t seconds, size_t *num_results, esp_hid_scan_result_t **results);
void esp_hid_scan_results_free(esp_hid_scan_result_t *results);
void print_uuid(esp_bt_uuid_t *uuid);
esp_err_t clear_saved_devices();
#ifdef __cplusplus
}
#endif

#endif /* _BL_FCTS_H_ */