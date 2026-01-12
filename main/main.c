#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"

#define TAG "LINKY"

// UART configuration
#define UART_PORT      UART_NUM_1
#define UART_RX_PIN    20         // CHANGE to your RX pin
#define UART_TX_PIN    UART_PIN_NO_CHANGE
#define UART_BUF_SIZE  1024

// TIC control characters
#define STX 0x02
#define ETX 0x03
#define LF  0x0A
#define CR  0x0D

static uint8_t tic_checksum(const char *label, const char *value)
{
    uint8_t sum = 0;
    const char *p;

    for (p = label; *p; p++) sum += *p;
    sum += ' ';
    for (p = value; *p; p++) sum += *p;
    sum += ' ';

    return (sum & 0x3F) + 0x20;
}

static void linky_task(void *arg)
{
    uint8_t data[UART_BUF_SIZE];
    int len;

    bool in_frame = false;
    char line[128];
    int line_pos = 0;

    while (1) {
        len = uart_read_bytes(UART_PORT, data, sizeof(data), pdMS_TO_TICKS(100));
        if (len <= 0) continue;

        for (int i = 0; i < len; i++) {
            uint8_t c = data[i];

            if (c == STX) {
                in_frame = true;
                line_pos = 0;
                ESP_LOGI(TAG, "STX");
                continue;
            }

            if (c == ETX) {
                in_frame = false;
                ESP_LOGI(TAG, "ETX");
                continue;
            }

            if (!in_frame) continue;

            if (c == LF) {
                line[line_pos] = '\0';

                char label[32], value[64], checksum_char;
                if (sscanf(line, "%31s %63s %c", label, value, &checksum_char) == 3) {
                    uint8_t cs = tic_checksum(label, value);
                    if (cs == checksum_char) {
                        ESP_LOGI(TAG, "OK: %s = %s", label, value);
                    } else {
                        ESP_LOGW(TAG, "BAD CS: %s %s (%c != %c)",
                                 label, value, checksum_char, cs);
                    }
                }

                line_pos = 0;
            } else if (c != CR) {
                if (line_pos < sizeof(line) - 1) {
                    line[line_pos++] = c;
                }
            }
        }
    }
}

void app_main(void)
{
    uart_config_t uart_config = {
        .baud_rate = 1200,
        .data_bits = UART_DATA_7_BITS,
        .parity    = UART_PARITY_EVEN,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, UART_BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    xTaskCreate(linky_task, "linky_task", 4096, NULL, 10, NULL);

    ESP_LOGI(TAG, "Linky TIC decoder started");
}

