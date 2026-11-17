#include "board.h"
int main(void) {
    board_init();
    uart_puts("RECEIVER READY\n");
    for (;;) {
        uint32_t id, format;
        uint8_t n, b[8];
        if (!can_receive(&id, &n, b, &format))
            continue;
        uart_puts("RX t=");
        uart_uint(board_ms());
        uart_puts(" id=");
        uart_uint(id);
        uart_puts(" dlc=");
        uart_uint(n);
        uart_puts(" format=");
        uart_uint(format);
        uart_puts(" raw=");
        for (unsigned i = 0; i < 8; i++)
            uart_hex(b[i]);
        uart_puts(" temp=");
        uart_int((int16_t)((uint16_t)b[0] | ((uint16_t)b[1] << 8)));
        uart_puts(" vibration=");
        uart_uint((uint16_t)b[2] | ((uint16_t)b[3] << 8));
        uart_puts(" flags=");
        uart_uint(b[4]);
        uart_puts(" seq=");
        uart_uint(b[5]);
        uart_puts("\n");
    }
}
