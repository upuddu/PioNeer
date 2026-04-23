#include <stdio.h>
#include "pico/stdlib.h"
#include "config.h"
#include "adc_joystick.h"
#include "gpio_buttons.h"
#include "self_test.h"
#include "ui.h"

int main(void)
{
    stdio_init_all();
    sleep_ms(2000);
    printf("=== BOOT ===\n");
    printf("=== PioNeer ===\n");

    printf("[INIT] Buttons...\n");
    buttons_init();

    printf("[INIT] Joystick...\n");
    joystick_init();
    joystick_sw_init();

    printf("[INIT] Display...\n");
    // Display initialization is handled by LVGL setup in display_self_test or similar
    // For now, assuming display is ready
    display_self_test();

    printf("[INIT] Menu...\n");
    main_menu_init();

    printf("[MAIN] Running menu.\n");
    while (true)
    {
        main_menu_run();
        sleep_ms(50);
    }
    return 0;
}