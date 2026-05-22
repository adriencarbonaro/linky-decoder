#include "uart.h"

#include "driver/uart.h"
#include "esp_log.h"
#include "sdkconfig.h"

/* Defines ------------------------------------------------------------------ */
#define TAG                   "uart"

#define UART_PORT             UART_NUM_1
#define UART_RX_PIN           CONFIG_UART_RX_PIN
#define UART_TX_PIN           UART_PIN_NO_CHANGE
#define UART_BUF_SIZE         1024

/* Global objects ----------------------------------------------------------- */
uart_data_handler_t uart_data_handler = NULL;

/* Static functions --------------------------------------------------------- */
static void uart_task(void* args)
{
    ESP_UNUSED(args);

    uint8_t data[UART_BUF_SIZE];
    int len;

    vTaskDelay(pdMS_TO_TICKS(10000));

    while (1)
    {
        len = uart_read_bytes(UART_PORT, data, sizeof(data), pdMS_TO_TICKS(100));
        if (len <= 0)
            continue;

        if (uart_data_handler) uart_data_handler(data, len);
    }
}

/* Public functions --------------------------------------------------------- */
void uart_init(const uart_data_handler_t callback)
{
    uart_config_t uart_config = {
        .baud_rate = 1200,
        .data_bits = UART_DATA_7_BITS,
        .parity    = UART_PARITY_EVEN,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, UART_BUF_SIZE * 2, 0, 0,
                                        NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    if (callback) uart_data_handler = callback;

    xTaskCreate(uart_task, "uart_task", 4096, NULL, 10, NULL);
}
