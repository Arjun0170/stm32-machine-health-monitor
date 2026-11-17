#ifndef BOARD_H
#define BOARD_H
#include <stdbool.h>
#include <stdint.h>
void board_init(void);
uint32_t board_ms(void);
void uart_puts(const char *s);
void uart_uint(uint32_t v);
void uart_int(int32_t v);
void uart_hex(uint8_t v);
int uart_get(void);
bool temperature_read(int16_t *value);
bool acceleration_read(int16_t xyz[3]);
bool can_send(const uint8_t data[8]);
bool can_receive(uint32_t *id, uint8_t *length, uint8_t data[8], uint32_t *format);
#endif
