#include "board.h"
int main(void) {
    board_init();
    uart_puts("PROBE BOOT\n");
    int16_t t = 0, xyz[3] = {0};
    bool i = temperature_read(&t), s = acceleration_read(xyz);
    uart_puts("PROBE I2C ok=");
    uart_uint(i);
    uart_puts(" temp=");
    uart_int(t);
    uart_puts(" SPI ok=");
    uart_uint(s);
    uart_puts(" xyz=");
    for (unsigned j = 0; j < 3; j++) {
        uart_int(xyz[j]);
        uart_puts(" ");
    }
    uart_puts("\n");
    const uint8_t b[8] = {0xfa, 0, 0x64, 0, 0, 0, 0, 0};
    while (board_ms() < 20) {
    }
    uart_puts(can_send(b) ? "PROBE CAN QUEUED\n" : "PROBE CAN BUSY\n");
    for (;;)
        if (uart_get() == 's')
            uart_puts("PROBE UART RX OK\n");
}
