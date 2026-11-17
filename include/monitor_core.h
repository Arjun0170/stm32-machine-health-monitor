#ifndef MONITOR_CORE_H
#define MONITOR_CORE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define VIBRATION_ALARM 1u
#define TEMPERATURE_FAULT 2u
#define CONSOLE_CAPACITY 64u
struct monitor {
    int16_t temperature;
    uint16_t vibration;
    uint16_t threshold;
    uint8_t flags;
    uint8_t sequence;
};
struct console {
    char line[CONSOLE_CAPACITY];
    size_t used;
    bool discard, invalid, cr;
};
enum response {
    RESPONSE_NONE,
    RESPONSE_STATUS,
    RESPONSE_THRESHOLD,
    ERROR_INVALID,
    ERROR_UNKNOWN,
    ERROR_OVERSIZED
};
void monitor_init(struct monitor *m);
void monitor_temperature(struct monitor *m, bool valid, int16_t value);
void monitor_acceleration(struct monitor *m, int16_t x, int16_t y);
void monitor_payload(const struct monitor *m, uint8_t data[8]);
void monitor_queued(struct monitor *m);
enum response monitor_command(struct monitor *m, const char *line);
enum response console_feed(struct console *c, struct monitor *m, unsigned char value);
#endif
