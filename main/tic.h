#ifndef TIC_H_
#define TIC_H_

#include <stdint.h>

/* Prototypes --------------------------------------------------------------- */
int tic_decode(uint8_t* data, int size);

/* Drop the value cache so the next parsed frame republishes every label.
 * Wire this to the MQTT connect callback to refresh HA after a broker
 * restart that lost retained messages. */
void tic_cache_reset(void);

#endif /* TIC_H_ */
