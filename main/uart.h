#ifndef UART_H_
#define UART_H_

#include <stdint.h>

/* Typedefs ----------------------------------------------------------------- */
typedef int (*uart_data_handler_t)(uint8_t* data, int size);

/* Prototypes --------------------------------------------------------------- */
void uart_init(const uart_data_handler_t callback);

#endif /* UART_H_ */
