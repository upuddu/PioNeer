#include "pico/stdlib.h"
#include "config.h"
#include "spi_display.h"

int main(void) {
    stdio_init_all();
    display_init();

    // Test 1: clear to red
    display_clear(COLOR_RED);
    sleep_ms(1000);

    // Test 2: clear to blue
    display_clear(COLOR_BLUE);
    sleep_ms(1000);

    // Test 3: draw some text
    display_clear(COLOR_BLACK);
    display_write_string(10, 10, "PioNeer!", COLOR_WHITE);
    display_write_string(10, 30, "SPI OK", COLOR_GREEN);

    while (true) {}
}
