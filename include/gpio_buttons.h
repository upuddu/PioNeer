#ifndef GPIO_BUTTONS_H
#define GPIO_BUTTONS_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    BTN_A = 0,
    BTN_B,
    BTN_X,
    BTN_Y
} Button;

typedef enum
{
    BTN_STATE_RELEASED = 0,
    BTN_STATE_PRESSED = 1,
    BTN_STATE_UNKNOWN = 2
} ButtonState;

typedef void (*button_callback_t)(Button btn, ButtonState state);

void buttons_init(void);
void buttons_set_callback(button_callback_t callback);
ButtonState button_get_state(Button btn);

#endif // GPIO_BUTTONS_H