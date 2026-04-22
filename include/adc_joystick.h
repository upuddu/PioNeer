#ifndef ADC_JOYSTICK_H
#define ADC_JOYSTICK_H

#include <stdint.h>

typedef struct {
    uint16_t x;   // 0–4095
    uint16_t y;   // 0–4095
} JoystickReading;

void joystick_init(void);
JoystickReading joystick_read(void);

#endif // ADC_JOYSTICK_H
