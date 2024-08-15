#include "esp_now_fcts.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"

#define SBUS_UART_NUM UART_NUM_2
#define SBUS_TXD_PIN (GPIO_NUM_17)
#define SBUS_RXD_PIN (GPIO_NUM_16)
#define SBUS_BAUD_RATE 115200

void sbus_task(void *pvParameters)
{
    uart_config_t uart_config = {
        .baud_rate = SBUS_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_2,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};

    uart_param_config(SBUS_UART_NUM, &uart_config);
    uart_set_pin(SBUS_UART_NUM, SBUS_TXD_PIN, SBUS_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    ESP_ERROR_CHECK(uart_set_line_inverse(SBUS_UART_NUM, UART_SIGNAL_RXD_INV));
    uart_driver_install(SBUS_UART_NUM, 1024, 0, 0, NULL, 0);

    uint8_t sbus_packet[25] = {0};

    while (1)
    {
        // Fill in the SBUS packet
        sbus_packet[0] = 0x0F; // SBUS header

        // Fill in the servo channels (11 bits each)
        for (int i = 0; i < 16; i++)
        {
            sbus_packet[i * 11 / 8 + 1 + i] = (send_data->hid_data[i] >> 3) & 0xFF;
            sbus_packet[i * 11 / 8 + 2 + i] = ((send_data->hid_data[i] & 0x07) << 5) | ((send_data->hid_data[i + 1] >> 6) & 0x1F);
            int parity = 0;
            for (int j = 0; j < 8; j++) {
                if (sbus_packet[i * 11 / 8 + 1 + i] & (1 << j)) {
                    parity ^= 1;
                }
            }
            // Set the parity bit in the LSB
            sbus_packet[i * 11 / 8 + 1 + i] |= (parity << 7);
        }

        // Fill in channel 17, channel 18, frame lost, and failsafe activated bits
        sbus_packet[23] = 0x00; // Initialize to 0
        // Set the bits based on your requirements
        // Bit 0: channel 17 (0x01)
        // Bit 1: channel 18 (0x02)
        // Bit 2: frame lost (0x04)
        // Bit 3: failsafe activated (0x08)

        sbus_packet[24] = 0x00; // SBUS footer

        // Send the SBUS packet over UART
        uart_write_bytes(SBUS_UART_NUM, (const char *)sbus_packet, sizeof(sbus_packet));

        // // Log the data if needed
                ESP_LOGI("INFO", "Received data  : " DATASTR " ", DATA2STR(send_data->hid_data));

        vTaskDelay(pdMS_TO_TICKS(20)); // SBUS frame rate is 7-14ms
    }
}

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
    xTaskCreate(sbus_task, "sbus_task", 2048, NULL, 10, NULL);
}
