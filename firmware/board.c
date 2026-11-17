#include "board.h"
#define REG(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define UART 0x40004800u
#define I2C 0x40005400u
#define SPI 0x40013400u
#define CAN 0x4000a000u
#define RAM 0x4000ac00u
#define POLL_LIMIT 10000u
static volatile uint32_t ticks;
void SysTick_Handler(void) {
    ticks++;
}
uint32_t board_ms(void) {
    return ticks;
}
static bool wait_flag(uint32_t addr, uint32_t mask) {
    for (uint32_t n = 0; n < POLL_LIMIT; n++)
        if (REG(addr) & mask)
            return true;
    return false;
}
void uart_puts(const char *s) {
    while (*s) {
        if (!wait_flag(UART + 0x1c, 1u << 7))
            return;
        REG(UART + 0x28) = (uint8_t)*s++;
    }
}
void uart_uint(uint32_t v) {
    char b[11];
    unsigned n = 0;
    do {
        b[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    while (n) {
        char c[2] = {b[--n], 0};
        uart_puts(c);
    }
}
void uart_int(int32_t v) {
    if (v < 0) {
        uart_puts("-");
        uart_uint((uint32_t)(-v));
    } else
        uart_uint((uint32_t)v);
}
void uart_hex(uint8_t v) {
    const char *h = "0123456789abcdef";
    char b[3] = {h[v >> 4], h[v & 15], 0};
    uart_puts(b);
}
int uart_get(void) {
    return (REG(UART + 0x1c) & (1u << 5)) ? (int)(REG(UART + 0x24) & 255) : -1;
}
void board_init(void) {
    REG(0xe000ed08) = 0x08000000;               // vector table
    REG(0x580244e8) |= (1u << 18) | (1u << 21); // USART3, I2C1
    REG(0x580244ec) |= 1u << 8;                 // FDCAN
    REG(0x580244f0) |= 1u << 13;                // SPI4
    REG(UART + 0x0c) = 556;                     // 115200 at 64 MHz
    REG(UART) = 1 | (1u << 2) | (1u << 3);
    REG(I2C + 0x10) = 0x10707dbc; // bounded polled, nominal 100kHz
    REG(I2C) = 1;
    REG(SPI + 0x08) = 7 | (4u << 28);          // 8 bits, /32
    REG(SPI + 0x0c) = (1u << 22) | (1u << 26); // master, software NSS
    REG(CAN + 0x18) = 1;                       // INIT
    (void)wait_flag(CAN + 0x18, 1);
    REG(CAN + 0x18) = 3; // INIT + CCE
    REG(CAN + 0x1c) = (3u << 25) | (7u << 16) | (10u << 8) | 3u;
    REG(CAN + 0x80) = 0;                  // accept unmatched standard frames into FIFO0
    REG(CAN + 0xa0) = 0x400 | (8u << 16); // RX FIFO0, 8 x 16 bytes
    REG(CAN + 0xbc) = 0;                  // RX data size 8
    REG(CAN + 0xc0) = 1u << 16;           // dedicated TX buffer at RAM offset 0
    REG(CAN + 0xc8) = 0;                  // TX data size 8
    REG(CAN + 0x18) = 0;                  // normal, no loopback
    REG(0xe000e014) = 64000 - 1;          // 1ms at platform SysTick 64MHz
    REG(0xe000e018) = 0;
    REG(0xe000e010) = 7;
}
static bool i2c_wait(uint32_t mask) {
    for (uint32_t n = 0; n < POLL_LIMIT; n++) {
        uint32_t s = REG(I2C + 0x18);
        if (s & (1u << 4))
            return false;
        if (s & mask)
            return true;
    }
    return false;
}
bool temperature_read(int16_t *value) {
    uint8_t b[2];
    bool ok = false;
    REG(I2C + 0x1c) = 0x3f38; // clear NACK/STOP/error flags
    REG(I2C + 4) = (0x48u << 1) | (1u << 16) | (1u << 13);
    if (!i2c_wait(1u << 1))
        goto done;
    REG(I2C + 0x28) = 0; // register address 0
    if (!i2c_wait(1u << 6))
        goto done;
    REG(I2C + 4) = (0x48u << 1) | (1u << 10) | (2u << 16) | (1u << 13) | (1u << 25);
    for (unsigned i = 0; i < 2; i++) {
        if (!i2c_wait(1u << 2))
            goto done;
        b[i] = (uint8_t)REG(I2C + 0x24);
    }
    if (!i2c_wait(1u << 5))
        goto done;
    *value = (int16_t)((uint16_t)b[0] | ((uint16_t)b[1] << 8));
    ok = true;
done:
    REG(I2C + 4) = 1u << 14; // bounded cleanup, release bus after timeout/NACK
    REG(I2C + 0x1c) = 0x3f38;
    return ok;
}
bool acceleration_read(int16_t xyz[3]) {
    uint8_t b[7];
    REG(SPI) = 0;
    REG(SPI + 4) = 7; // command + six data bytes; transaction boundary -> CS finish
    REG(SPI + 0x18) = 0x818;
    REG(SPI) = 1 | (1u << 9);
    for (unsigned i = 0; i < 7; i++) {
        REG(SPI + 0x20) = i ? 0 : 0x80; // bit7 READ, address0
        if (!wait_flag(SPI + 0x14, 1)) {
            REG(SPI) = 0;
            return false;
        }
        b[i] = (uint8_t)REG(SPI + 0x30);
    }
    bool ok = wait_flag(SPI + 0x14, 1u << 3);
    REG(SPI) = 0;
    for (unsigned i = 0; i < 3; i++)
        xyz[i] = (int16_t)((uint16_t)b[1 + 2 * i] | ((uint16_t)b[2 + 2 * i] << 8));
    return ok;
}
bool can_send(const uint8_t data[8]) {
    if (REG(CAN + 0xcc) & 1)
        return false; // TXBRP buffer occupied
    REG(RAM) = 0x180u << 18;
    REG(RAM + 4) = 8u << 16; // DLC8, classical, standard, data frame
    for (unsigned i = 0; i < 2; i++)
        REG(RAM + 8 + 4 * i) = (uint32_t)data[4 * i] | ((uint32_t)data[4 * i + 1] << 8) |
                               ((uint32_t)data[4 * i + 2] << 16) |
                               ((uint32_t)data[4 * i + 3] << 24);
    REG(CAN + 0xd0) = 1; // TXBAR
    return true;
}
bool can_receive(uint32_t *id, uint8_t *length, uint8_t data[8], uint32_t *format) {
    uint32_t s = REG(CAN + 0xa4);
    if (!(s & 127))
        return false;
    uint32_t index = (s >> 8) & 63, addr = RAM + 0x400 + 16 * index;
    uint32_t h = REG(addr), h1 = REG(addr + 4);
    *id = (h >> 18) & 0x7ff;
    *length = (uint8_t)((h1 >> 16) & 15);
    *format = (h & 0x60000000u) | (h1 & 0x00300000u); // extended, RTR, FD, BRS
    for (unsigned i = 0; i < 2; i++) {
        uint32_t w = REG(addr + 8 + 4 * i);
        for (unsigned j = 0; j < 4; j++)
            data[i * 4 + j] = (uint8_t)(w >> (j * 8));
    }
    REG(CAN + 0xa8) = index;
    return true;
}
