#include "board.h"
#include "monitor_core.h"
static struct monitor state;
static struct console console;
static void print_status(void) {
    uart_puts("STATUS t=");
    uart_uint(board_ms());
    uart_puts(" temp=");
    uart_int(state.temperature);
    uart_puts(" vibration=");
    uart_uint(state.vibration);
    uart_puts(" threshold=");
    uart_uint(state.threshold);
    uart_puts(" flags=");
    uart_uint(state.flags);
    uart_puts("\n");
}
static void console_poll(void) {
    // Bound input work per loop so a continuous console cannot starve sampling.
    for (unsigned i = 0; i < 16; i++) {
        int ch = uart_get();
        if (ch < 0)
            return;
        switch (console_feed(&console, &state, (unsigned char)ch)) {
        case RESPONSE_STATUS:
            print_status();
            break;
        case RESPONSE_THRESHOLD:
            uart_puts("OK threshold=");
            uart_uint(state.threshold);
            uart_puts("\n");
            break;
        case ERROR_INVALID:
            uart_puts("ERR invalid command or threshold (100..4000 mg)\n");
            break;
        case ERROR_UNKNOWN:
            uart_puts("ERR unknown command\n");
            break;
        case ERROR_OVERSIZED:
            uart_puts("ERR command too long (max 63 bytes)\n");
            break;
        case RESPONSE_NONE:
            break;
        }
    }
}
static bool telemetry_queue(void) {
    uint8_t b[8];
    monitor_payload(&state, b);
    if (!can_send(b))
        return false;
    monitor_queued(&state);
    return true;
}
int main(void) {
    board_init();
    monitor_init(&state);
    uart_puts("MACHINE HEALTH MONITOR STM32H743 READY\n");
    uint32_t spi_due = 100, temp_due = 500, can_due = 500;
    bool event_pending = false, periodic_pending = true;
    uint8_t old_flags = 0;
    for (;;) {
        console_poll();
        uint32_t now = board_ms();
        if ((int32_t)(now - spi_due) >= 0) {
            spi_due += 100;
            int16_t xyz[3];
            if (acceleration_read(xyz))
                monitor_acceleration(&state, xyz[0], xyz[1]);
        }
        if ((int32_t)(now - temp_due) >= 0) {
            temp_due += 500;
            int16_t value = 0;
            bool valid = temperature_read(&value);
            monitor_temperature(&state, valid, value);
        }
        if (old_flags != state.flags) {
            event_pending = true;
            old_flags = state.flags;
        }
        if ((int32_t)(now - can_due) >= 0) {
            can_due += 500;
            periodic_pending = true;
        }
        // Event plus periodic are separate frames even if deadlines coincide.
        if (event_pending) {
            if (telemetry_queue())
                event_pending = false;
        } else if (periodic_pending && telemetry_queue())
            periodic_pending = false;
    }
}
