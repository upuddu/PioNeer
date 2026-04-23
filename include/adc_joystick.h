#ifndef ADC_JOYSTICK_H
#define ADC_JOYSTICK_H

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    uint16_t x;
    uint16_t y;
} JoystickReading;

void joystick_init(void);
JoystickReading joystick_read(void);
void joystick_sw_init(void);
bool joystick_sw_consume(void); // returns true once per press, clears flag

#endif