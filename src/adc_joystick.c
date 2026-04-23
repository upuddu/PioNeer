#include "adc_joystick.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"

#define JOYSTICK_X_PIN  40
#define JOYSTICK_Y_PIN  41
#define JOYSTICK_X_CH   0
#define JOYSTICK_Y_CH   1

void joystick_init(void) {
    adc_init();
    adc_gpio_init(JOYSTICK_X_PIN);
    adc_gpio_init(JOYSTICK_Y_PIN);
}

JoystickReading joystick_read(void) {
    JoystickReading r;
    adc_select_input(JOYSTICK_X_CH);
    r.x = adc_read();
    adc_select_input(JOYSTICK_Y_CH);
    r.y = adc_read();
    return r;
}