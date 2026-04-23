#ifndef CAR_GAME_H
#define CAR_GAME_H

#include <stdint.h>
#include <stdbool.h>

void car_game_init(void);
void car_game_update(void);
void car_button_callback(uint8_t btn, bool pressed);

#endif // CAR_GAME_H
