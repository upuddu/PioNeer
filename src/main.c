#include <stdio.h>
#include "pico/stdlib.h"
#include "gpio_buttons.h"

static const char *button_names[] = {"BTN_A", "BTN_B", "BTN_X", "BTN_Y"};

void button_event_handler(Button btn, ButtonState state) {
    const char *state_str = (state == BTN_STATE_PRESSED) ? "pressed" : "released";
    printf("%s %s\n", button_names[btn], state_str);
}

int main(void) {
    stdio_init_all();

    // ── Init buttons with interrupt-driven debouncing ─────────────────────────
    printf("Button test mode enabled (interrupt-driven). Press buttons A, B, X, Y...\n");
    buttons_init();
    buttons_set_callback(button_event_handler);

    // ── Main loop (no blocking, callbacks handle button events) ────────────────
    while (true) {
        sleep_ms(100);  // Low-power idle
    }

    return 0;
}
