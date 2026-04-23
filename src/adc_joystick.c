#include "adc_joystick.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

#define JOYSTICK_X_PIN 40
#define JOYSTICK_Y_PIN 41
#define JOYSTICK_X_CH 0
#define JOYSTICK_Y_CH 1
#define JOYSTICK_SW_PIN 39
#define DEBOUNCE_MS 50

static volatile bool sw_pressed = false;
static volatile uint32_t last_press_ms = 0;

static void sw_isr(uint gpio, uint32_t events)
{
    gpio_acknowledge_irq(gpio, events);
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - last_press_ms > DEBOUNCE_MS)
    {
        sw_pressed = true;
        last_press_ms = now;
    }
}

void joystick_init(void)
{
    adc_init();
    adc_gpio_init(JOYSTICK_X_PIN);
    adc_gpio_init(JOYSTICK_Y_PIN);
}

JoystickReading joystick_read(void)
{
    JoystickReading r;
    adc_select_input(JOYSTICK_X_CH);
    r.x = adc_read();
    adc_select_input(JOYSTICK_Y_CH);
    r.y = adc_read();
    return r;
}

void joystick_sw_init(void)
{
    gpio_init(JOYSTICK_SW_PIN);
    gpio_set_dir(JOYSTICK_SW_PIN, GPIO_IN);
    gpio_pull_up(JOYSTICK_SW_PIN);
    gpio_set_irq_enabled_with_callback(JOYSTICK_SW_PIN,
                                       GPIO_IRQ_EDGE_FALL, true, sw_isr);
    irq_set_enabled(IO_IRQ_BANK0, true);
}

bool joystick_sw_consume(void)
{
    if (sw_pressed)
    {
        sw_pressed = false;
        return true;
    }
    return false;
}