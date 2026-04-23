#include "pico/stdlib.h"
#include "adc_joystick.h"
#include <stdio.h>
#include <math.h>

int main(void)
{
    stdio_init_all();
    joystick_init();
    joystick_sw_init();

    while (true)
    {
        JoystickReading joy = joystick_read();
        int dx = (int)joy.x - 2048;
        int dy = (int)joy.y - 2048;

        if (dx > -100 && dx < 100 && dy > -100 && dy < 100)
        {
            printf("CENTER\n");
        }
        else
        {
            float angle = atan2f((float)dy, (float)dx) * 180.0f / M_PI;
            if (angle < 0)
                angle += 360.0f;
            printf("Angle: %.1f deg\n", angle);
        }

        if (joystick_sw_consume())
        {
            printf("CLICK\n");
        }

        sleep_ms(100);
    }
}