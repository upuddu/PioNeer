#ifndef AUDIO_MAZE_H
#define AUDIO_MAZE_H

#include <stdint.h>
#include <stdbool.h>

void audio_maze_init(void);
void audio_maze_update(void);
void audio_maze_button_callback(uint8_t btn, bool pressed);

#endif // AUDIO_MAZE_H
