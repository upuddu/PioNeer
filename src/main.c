#include "pico/stdlib.h"
#include "tictactoe.h"

int main(void) {
    stdio_init_all();
    
    // Initialize the game (includes display, audio, and joystick init)
    tictactoe_init();

    while (true) {
        // Run the game loop
        tictactoe_update();
        
        // Small delay to prevent CPU hogging
        sleep_ms(5);
    }
    return 0;
}