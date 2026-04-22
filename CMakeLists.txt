cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(PioNeer C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# Collect all source files
file(GLOB_RECURSE SOURCES
    "src/*.c"
    "games/*.c"
)

add_executable(PioNeer
    src/main.c
    src/adc_joystick.c
    src/spi_display.c
    src/i2c_dac.c
    src/pio_neopixel.c
    src/gpio_buttons.c
    games/lunar_lander.c
    games/car_racing.c
    games/tank_battle.c
)

# Generate PIO header from .pio file
pico_generate_pio_header(PioNeer ${CMAKE_CURRENT_LIST_DIR}/src/neopixel.pio)

target_include_directories(PioNeer PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(PioNeer
    pico_stdlib
    hardware_adc
    hardware_spi
    hardware_i2c
    hardware_pio
    hardware_dma
    hardware_gpio
    hardware_timer
    hardware_irq
)

pico_enable_stdio_usb(PioNeer 1)
pico_enable_stdio_uart(PioNeer 0)
pico_add_extra_outputs(PioNeer)
