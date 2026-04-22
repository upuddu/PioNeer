#include "adc_joystick.h"
#include "config.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"

void joystick_init(void) {
    adc_init();
    adc_gpio_init(JOYSTICK_X_PIN);  // GP26 → ADC ch0
    adc_gpio_init(JOYSTICK_Y_PIN);  // GP27 → ADC ch1
}

JoystickReading joystick_read(void) {
    JoystickReading r;

    adc_select_input(JOYSTICK_X_CH);
    r.x = adc_read();   // 12-bit: 0–4095

    adc_select_input(JOYSTICK_Y_CH);
    r.y = adc_read();

    return r;
}
