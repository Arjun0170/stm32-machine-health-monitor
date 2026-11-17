#include "monitor_core.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void expect_bytes(const uint8_t *a, const uint8_t *b) {
    assert(memcmp(a, b, 8) == 0);
}
static enum response line(struct console *c, struct monitor *m, const char *s) {
    enum response r = RESPONSE_NONE;
    for (; *s; s++) {
        enum response next = console_feed(c, m, (unsigned char)*s);
        if (next != RESPONSE_NONE)
            r = next;
    }
    return r;
}
static void test_commands(void) {
    struct monitor m;
    monitor_init(&m);
    struct console c = {0};
    assert(m.threshold == 1500);
    assert(monitor_command(&m, "status") == RESPONSE_STATUS);
    const char *bad[] = {
        "threshold",      "threshold ",     "threshold 99",
        "threshold 4001", "threshold -100", "threshold +100",
        "threshold 100x", "threshold 100 ", "threshold 10000000000000000000000000000000000000000",
        "threshold 1 00", "threshold  100"};
    for (unsigned i = 0; i < sizeof(bad) / sizeof(*bad); i++) {
        assert(monitor_command(&m, bad[i]) == ERROR_INVALID);
        assert(m.threshold == 1500);
    }
    assert(monitor_command(&m, "threshold 100") == RESPONSE_THRESHOLD && m.threshold == 100);
    assert(monitor_command(&m, "threshold 4000") == RESPONSE_THRESHOLD && m.threshold == 4000);
    assert(monitor_command(&m, "") == ERROR_UNKNOWN);
    assert(monitor_command(&m, "hello") == ERROR_UNKNOWN);
    assert(line(&c, &m, "status\r\n") == RESPONSE_STATUS);
    for (unsigned i = 0; i < 64; i++)
        assert(console_feed(&c, &m, 'a') == RESPONSE_NONE);
    assert(console_feed(&c, &m, '\n') == ERROR_OVERSIZED);
    assert(line(&c, &m, "threshold 1200\n") == RESPONSE_THRESHOLD && m.threshold == 1200);
    assert(console_feed(&c, &m, 0) == RESPONSE_NONE);
    assert(console_feed(&c, &m, '\n') == ERROR_INVALID);
    assert(line(&c, &m, "status\n") == RESPONSE_STATUS);
}
static void test_alarm(void) {
    struct monitor m;
    monitor_init(&m);
    monitor_acceleration(&m, -1500, 1499);
    assert(m.vibration == 1500 && m.flags == 0);
    monitor_acceleration(&m, -1501, 0);
    assert(m.flags == VIBRATION_ALARM);
    monitor_acceleration(&m, 1500, -1500);
    assert(m.flags == 0);
    monitor_acceleration(&m, 0, INT16_MIN);
    assert(m.vibration == 32768 && m.flags == 1);
    monitor_acceleration(&m, 101, 0);
    assert(m.flags == 0);
    monitor_command(&m, "threshold 100");
    assert(m.flags == 1);
    monitor_command(&m, "threshold 4000");
    assert(m.flags == 0);
}
static void test_temperature_and_payload(void) {
    struct monitor m;
    uint8_t b[8];
    monitor_init(&m);
    monitor_temperature(&m, false, 123);
    monitor_payload(&m, b);
    const uint8_t first[8] = {0, 0, 0, 0, 2, 0, 0, 0};
    expect_bytes(b, first);
    monitor_temperature(&m, true, -123);
    monitor_acceleration(&m, -900, 200);
    monitor_command(&m, "threshold 400");
    m.sequence = 255;
    monitor_temperature(&m, false, 999);
    monitor_payload(&m, b);
    const uint8_t want[8] = {0x85, 0xff, 0x84, 0x03, 3, 255, 0, 0};
    expect_bytes(b, want);
    assert(m.temperature == -123);
    assert(m.sequence == 255);
    monitor_queued(&m);
    assert(m.sequence == 0);
    monitor_temperature(&m, true, 250);
    assert(m.flags == 1 && m.temperature == 250);
    monitor_acceleration(&m, 400, 0);
    assert(m.flags == 0);
}
int main(void) {
    test_commands();
    test_alarm();
    test_temperature_and_payload();
    puts("PASS commands, boundaries, oversized recovery, alarm transitions, INT16_MIN, temperature "
         "recovery, exact little-endian payload, sequence wrap");
}
