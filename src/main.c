#include "pico/stdlib.h"
#include "gpio_buttons.h"

int main(void) {
    stdio_init_all();

    // ── Init buttons ──────────────────────────────────────────────────────────
    buttons_init();
    printf("Button test mode enabled. Press buttons A, B, X, Y...\n");

    // ── Button testing loop ───────────────────────────────────────────────────
    uint32_t last_state = 0;
    
    while (true) {
        uint32_t current_state = 0;
        
        if (button_is_pressed(BTN_A)) current_state |= (1 << BTN_A);
        if (button_is_pressed(BTN_B)) current_state |= (1 << BTN_B);
        if (button_is_pressed(BTN_X)) current_state |= (1 << BTN_X);
        if (button_is_pressed(BTN_Y)) current_state |= (1 << BTN_Y);
        
        // Detect state changes (debounce with simple polling)
        if (current_state != last_state) {
            if (current_state & (1 << BTN_A)) printf("BTN_A pressed\n");
            if (current_state & (1 << BTN_B)) printf("BTN_B pressed\n");
            if (current_state & (1 << BTN_X)) printf("BTN_X pressed\n");
            if (current_state & (1 << BTN_Y)) printf("BTN_Y pressed\n");
            
            if (!(current_state & (1 << BTN_A)) && (last_state & (1 << BTN_A))) printf("BTN_A released\n");
            if (!(current_state & (1 << BTN_B)) && (last_state & (1 << BTN_B))) printf("BTN_B released\n");
            if (!(current_state & (1 << BTN_X)) && (last_state & (1 << BTN_X))) printf("BTN_X released\n");
            if (!(current_state & (1 << BTN_Y)) && (last_state & (1 << BTN_Y))) printf("BTN_Y released\n");
            
            last_state = current_state;
        }
        
        sleep_ms(50);  // Simple debounce
    }

    return 0;
}
