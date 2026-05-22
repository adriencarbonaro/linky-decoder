#include "ha_entities.h"

#include <stdint.h>

#include "esp_err.h"
#include "ha.h"
#include "led_strip.h"

#define ARRAY_DIM(x) (sizeof((x)) / sizeof((x)[0]))

/* Linky Decoder Home Assistant entities ------------------------------------
 *
 * One ha_entity_t per TIC label that tic.c publishes (via ha_publish, keyed
 * by the lowercased label = entity id). The ha lib wraps these with the
 * device / origin / availability blocks and adds a version sensor for free.
 * -------------------------------------------------------------------------- */

/* value_template drops empty / zero readings so HA's total_increasing delta
 * calc doesn't see a 0 -> real-index jump as one day's energy. */
#define ENERGY_INDEX_TEMPLATE \
    "{% if value and value|int(0) > 0 %}{{ value }}{% endif %}"

static const ha_entity_t s_entities[] = {
    {
        .id = "adco",
        .name = "Compteur ID",
        .platform = HA_SENSOR,
        .state_topic = "ADCO",
        .entity_category = "diagnostic",
        .icon = "mdi:identifier",
    },
    {
        .id = "optarif",
        .name = "Tariff Option",
        .platform = HA_SENSOR,
        .state_topic = "OPTARIF",
        .entity_category = "diagnostic",
        .icon = "mdi:cash-multiple",
    },
    {
        .id = "isousc",
        .name = "Subscribed Current",
        .platform = HA_SENSOR,
        .state_topic = "ISOUSC",
        .device_class = "current",
        .unit = "A",
        .entity_category = "diagnostic",
    },
    {
        .id = "hchc",
        .name = "Off-peak Index",
        .platform = HA_SENSOR,
        .state_topic = "HCHC",
        .device_class = "energy",
        .unit = "Wh",
        .state_class = "total_increasing",
        .value_template = ENERGY_INDEX_TEMPLATE,
        .icon = "mdi:counter",
    },
    {
        .id = "hchp",
        .name = "Peak Index",
        .platform = HA_SENSOR,
        .state_topic = "HCHP",
        .device_class = "energy",
        .unit = "Wh",
        .state_class = "total_increasing",
        .value_template = ENERGY_INDEX_TEMPLATE,
        .icon = "mdi:counter",
    },
    {
        .id = "ptec",
        .name = "Current Period",
        .platform = HA_SENSOR,
        .state_topic = "PTEC",
        .icon = "mdi:clock-outline",
    },
    {
        .id = "iinst",
        .name = "Instant Current",
        .platform = HA_SENSOR,
        .state_topic = "IINST",
        .device_class = "current",
        .unit = "A",
        .state_class = "measurement",
    },
    {
        .id = "imax",
        .name = "Max Current",
        .platform = HA_SENSOR,
        .state_topic = "IMAX",
        .device_class = "current",
        .unit = "A",
        .entity_category = "diagnostic",
    },
    {
        .id = "papp",
        .name = "Apparent Power",
        .platform = HA_SENSOR,
        .state_topic = "PAPP",
        .device_class = "apparent_power",
        .unit = "VA",
        .state_class = "measurement",
    },
    {
        .id = "hhphc",
        .name = "HP/HC Schedule",
        .platform = HA_SENSOR,
        .state_topic = "HHPHC",
        .entity_category = "diagnostic",
        .icon = "mdi:calendar-clock",
    },
    {
        .id = "adps",
        .name = "Overcurrent Warning",
        .platform = HA_SENSOR,
        .state_topic = "ADPS",
        .device_class = "current",
        .unit = "A",
        .entity_category = "diagnostic",
        .icon = "mdi:alert",
    },
    {
        .id = "motdetat",
        .name = "Status Word",
        .platform = HA_SENSOR,
        .state_topic = "MOTDETAT",
        .entity_category = "diagnostic",
        .icon = "mdi:information-outline",
    },
};

const ha_entity_t* get_entities(uint16_t* nb_entities)
{
    *nb_entities = ARRAY_DIM(s_entities);
    return s_entities;
}
