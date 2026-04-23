#ifndef GPIO_BUTTONS_H
#define GPIO_BUTTONS_H

#include <stdbool.h>

typedef enum {
    BTN_A = 0,
    BTN_B,
    BTN_X,
    BTN_Y
} Button;

void buttons_init(void);
void buttons_enable_interrupts(void);
bool button_is_pressed(Button btn);

#endif // GPIO_BUTTONS_H
