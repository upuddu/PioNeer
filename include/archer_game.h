#ifndef ARCHER_GAME_H
#define ARCHER_GAME_H

#include <stdint.h>
#include <stdbool.h>

void archer_game_init(void);
void archer_game_update(void);
void archer_button_callback(uint8_t btn, bool pressed);

#endif // ARCHER_GAME_H
