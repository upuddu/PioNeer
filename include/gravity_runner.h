#ifndef GRAVITY_RUNNER_H
#define GRAVITY_RUNNER_H

#include <stdint.h>
#include <stdbool.h>

void gravity_runner_init(void);
void gravity_runner_update(void);
void gravity_runner_button_callback(uint8_t btn, bool pressed);

#endif // GRAVITY_RUNNER_H
