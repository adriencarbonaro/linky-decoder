#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "ha.h"
#include "ha_entities.h"
#include "mqtt.h"
#include "tic.h"
#include "uart.h"
#include "version.h"
#include "wifi.h"

#define TAG "LINKY"

static const ha_identity_t identity = {
    .device_id = CONFIG_DEVICE_ID,
    .device_name = CONFIG_DEVICE_NAME,
    .manufacturer = CONFIG_MANUFACTURER,
    .model = CONFIG_MODEL,
    .version_str = DESCRIBE,
};

static void on_mqtt_connect(void)
{
    /* (Re)publish HA discovery, then drop the TIC value cache so the next
     * parsed frame republishes every label - covers a broker that lost its
     * retained messages (restart without persistence). */
    ha_publish_discovery();
    tic_cache_reset();
}

void app_main(void)
{
    /* Init Home Assistant layer */
    uint16_t nb_entities = 0;
    const ha_entity_t* entities = get_entities(&nb_entities);
    ha_init(&identity, entities, nb_entities);

    /* TIC decoder on the UART. */
    uart_init(tic_decode);

    /* Connectivity: wifi + mqtt are the project's to drive. The HA layer
     * owns the availability topic convention, so it feeds the LWT. */
    EventGroupHandle_t wifi_event_group = xEventGroupCreate();
    wifi_init(wifi_event_group);
    mqtt_set_on_connect(on_mqtt_connect);
    mqtt_start(wifi_event_group, ha_availability_topic());
}
