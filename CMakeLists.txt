cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(pi_pico_scope C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(pi-pico-scope
    src/main.c
    src/bsp.c
    src/gfx.c
    src/panel_st7789.c
    src/scope.c
    src/wave.c
    src/ui.c
)

target_include_directories(pi-pico-scope PRIVATE src)

target_link_libraries(pi-pico-scope
    pico_stdlib
    pico_platform
    hardware_adc
    hardware_dma
    hardware_gpio
    hardware_spi
    hardware_sync
    hardware_timer
    hardware_uart
    hardware_watchdog
    pico_stdio
    pico_stdio_usb
    tinyusb_device
)

pico_enable_stdio_usb(pi-pico-scope 1)
pico_enable_stdio_uart(pi-pico-scope 0)

pico_add_extra_outputs(pi-pico-scope)