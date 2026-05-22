#include "tic.h"

#include "esp_log.h"
#include "ha.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* Defines ------------------------------------------------------------------ */
#define TAG "tic"

#define STX 0x02
#define ETX 0x03
#define LF 0x0A
#define CR 0x0D

#define tic_checksum(...) tic_checksum_historique(__VA_ARGS__)

/* Per-label cache for dedupe-at-source. Most TIC labels (ADCO, OPTARIF, HCHC,
 * HCHP, ...) hold the same value frame after frame for minutes-to-hours;
 * republishing them every ~1.5 s spams the broker and stamps HA's "last
 * updated" every cycle. We only publish when the value differs from the
 * last one we sent. The cache is RAM-only, so a reboot or fresh flash
 * forces a full republish; for warm MQTT reconnects the broker's retained
 * values keep HA up to date without us having to republish anything. */
#define TIC_LABEL_MAX 16  /* longest historique label is 8 chars + slack */
#define TIC_VALUE_MAX 32  /* index values are up to ~9 digits */
#define TIC_CACHE_SIZE 40 /* upper bound on labels per frame */

typedef struct
{
    char label[TIC_LABEL_MAX];
    char value[TIC_VALUE_MAX];
} tic_cache_entry_t;

static tic_cache_entry_t s_cache[TIC_CACHE_SIZE];
static uint8_t s_cache_count = 0;

/* Static functions --------------------------------------------------------- */
static uint8_t tic_checksum_historique(const char* label, const char* value)
{
    uint8_t sum = 0;

    for (const char* p = label; *p; p++)
    {
        sum += *p;
    }

    sum += 0x20;  // SP between label and value

    for (const char* p = value; *p; p++)
    {
        sum += *p;
    }

    return (sum & 0x3F) + 0x20;
}

/* Returns true if value is new or differs from the last cached one for this
 * label, false if it matches what we already published. Updates the cache
 * on a change so the next comparison is against the freshest sent value. */
static bool tic_value_changed(const char* label, const char* value)
{
    for (uint8_t i = 0; i < s_cache_count; i++)
    {
        if (strcmp(s_cache[i].label, label) != 0) continue;

        if (strcmp(s_cache[i].value, value) == 0) return false;

        strncpy(s_cache[i].value, value, TIC_VALUE_MAX - 1);
        s_cache[i].value[TIC_VALUE_MAX - 1] = '\0';
        return true;
    }

    /* New label: cache it (if there's room) and publish. */
    if (s_cache_count < TIC_CACHE_SIZE)
    {
        strncpy(s_cache[s_cache_count].label, label, TIC_LABEL_MAX - 1);
        s_cache[s_cache_count].label[TIC_LABEL_MAX - 1] = '\0';
        strncpy(s_cache[s_cache_count].value, value, TIC_VALUE_MAX - 1);
        s_cache[s_cache_count].value[TIC_VALUE_MAX - 1] = '\0';
        s_cache_count++;
    }
    return true;
}

/* Public functions --------------------------------------------------------- */
void tic_cache_reset(void) { s_cache_count = 0; }

int tic_decode(uint8_t* data, int len)
{
    bool in_frame = false;
    char line[128];
    int line_pos = 0;

    for (int i = 0; i < len; i++)
    {
        uint8_t c = data[i];

        if (c == STX)
        {
            in_frame = true;
            line_pos = 0;
            ESP_LOGI(TAG, "STX");
            continue;
        }

        if (c == ETX)
        {
            in_frame = false;
            ESP_LOGI(TAG, "ETX");
            continue;
        }

        if (!in_frame) continue;

        if (c == LF)
        {
            line[line_pos] = '\0';
            line_pos = 0;

            /* Parse LABEL */
            char* p1 = strchr(line, ' ');
            if (!p1) continue;
            *p1 = '\0';
            char* label = line;
            char* value = p1 + 1;

            /* Look for checksum */
            char* p2 = strchr(value, ' ');
            bool has_checksum = false;
            uint8_t checksum_char = 0;
            if (p2)
            {
                has_checksum = true;
                *p2 = '\0';
                checksum_char = *(p2 + 1);
            }

            /* Validate checksum */
            bool cs_ok = true;
            if (has_checksum)
            {
                uint8_t cs = tic_checksum(label, value);
                cs_ok = (cs == checksum_char);

                if (!cs_ok)
                {
                    ESP_LOGW(TAG,
                             "BAD CS: %s %s (%c != %c)",
                             label,
                             value,
                             checksum_char,
                             cs);
                }
            }

            /* Drop anything that would publish a bogus value: a failed
             * checksum, or a value field that ended up empty (line truncated
             * by a spurious LF / chunked frame). HA's total_increasing state
             * class otherwise interprets the drop-to-zero as a meter reset
             * and fabricates a huge daily delta. */
            if (!cs_ok) continue;
            if (!*value) continue;
            if (!tic_value_changed(label, value)) continue;

            ESP_LOGI(TAG, "OK: %s = %s", label, value);

            /* HA entity ids are the lowercased TIC label (see ha_entities.c).
             * ha_publish() resolves the topic and silently ignores labels
             * that have no registered entity. */
            char id[TIC_LABEL_MAX];
            size_t n = 0;
            for (; label[n] && n < sizeof(id) - 1; n++)
                id[n] = (char)tolower((unsigned char)label[n]);
            id[n] = '\0';

            ha_publish(id, value);
        }
        else if (c != CR)
        {
            if (line_pos < sizeof(line) - 1) line[line_pos++] = c;
        }
    }

    return 0;
}
