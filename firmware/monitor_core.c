#include "monitor_core.h"
static bool equal(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}
static bool prefix(const char *s, const char *p) {
    while (*p)
        if (*s++ != *p++)
            return false;
    return true;
}
static void alarm_update(struct monitor *m) {
    if (m->vibration > m->threshold)
        m->flags |= VIBRATION_ALARM;
    else
        m->flags &= (uint8_t)~VIBRATION_ALARM;
}
void monitor_init(struct monitor *m) {
    m->temperature = 0;
    m->vibration = 0;
    m->threshold = 1500;
    m->flags = 0;
    m->sequence = 0;
}
void monitor_temperature(struct monitor *m, bool valid, int16_t value) {
    if (valid) {
        m->temperature = value;
        m->flags &= (uint8_t)~TEMPERATURE_FAULT;
    } else
        m->flags |= TEMPERATURE_FAULT;
}
void monitor_acceleration(struct monitor *m, int16_t x, int16_t y) {
    // Promote before negation: abs(INT16_MIN) is 32768 mg, never -32768.
    uint32_t ax = x < 0 ? (uint32_t)(-(int32_t)x) : (uint32_t)x;
    uint32_t ay = y < 0 ? (uint32_t)(-(int32_t)y) : (uint32_t)y;
    m->vibration = (uint16_t)(ax > ay ? ax : ay);
    alarm_update(m);
}
void monitor_payload(const struct monitor *m, uint8_t data[8]) {
    uint16_t t = (uint16_t)m->temperature;
    data[0] = (uint8_t)t;
    data[1] = (uint8_t)(t >> 8);
    data[2] = (uint8_t)m->vibration;
    data[3] = (uint8_t)(m->vibration >> 8);
    data[4] = m->flags;
    data[5] = m->sequence;
    data[6] = 0;
    data[7] = 0;
}
void monitor_queued(struct monitor *m) {
    m->sequence++;
}
enum response monitor_command(struct monitor *m, const char *line) {
    if (equal(line, "status"))
        return RESPONSE_STATUS;
    if (prefix(line, "threshold ")) {
        const char *p = line + 10;
        uint32_t v = 0;
        if (!*p)
            return ERROR_INVALID;
        for (; *p; p++) {
            if (*p < '0' || *p > '9' || v > 4000)
                return ERROR_INVALID;
            v = v * 10 + (uint32_t)(*p - '0');
        }
        if (v < 100 || v > 4000)
            return ERROR_INVALID;
        m->threshold = (uint16_t)v;
        alarm_update(m);
        return RESPONSE_THRESHOLD;
    }
    if (equal(line, "threshold"))
        return ERROR_INVALID;
    return ERROR_UNKNOWN;
}
enum response console_feed(struct console *c, struct monitor *m, unsigned char value) {
    if (value == '\n' && c->cr) {
        c->cr = false;
        return RESPONSE_NONE;
    }
    c->cr = value == '\r';
    if (value == '\n' || value == '\r') {
        c->line[c->used] = 0;
        enum response r = c->discard   ? ERROR_OVERSIZED
                          : c->invalid ? ERROR_INVALID
                                       : monitor_command(m, c->line);
        c->used = 0;
        c->discard = false;
        c->invalid = false;
        return r;
    }
    if (c->discard)
        return RESPONSE_NONE;
    if (c->used == CONSOLE_CAPACITY - 1) {
        c->discard = true;
        return RESPONSE_NONE;
    }
    if (value < 32 || value > 126)
        c->invalid = true;
    c->line[c->used++] = (char)value;
    return RESPONSE_NONE;
}
