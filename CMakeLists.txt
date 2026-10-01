cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(pi_pico_scope C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

option(PI_PICO_SCOPE_PANEL_PROBE "Run ST7789 display probe at boot" OFF)
option(PI_PICO_SCOPE_SLOW_SPI_PROBE "Use 250 kHz software SPI for the panel probe" OFF)
option(PI_PICO_SCOPE_REFERENCE_SPI_PROBE "Use 20 MHz hardware SPI0 for the panel probe" OFF)
option(PI_PICO_SCOPE_CONSTANT_COLOR_TEST "Run the minimal reference color loop" OFF)
option(PI_PICO_SCOPE_DUAL_ADC "Capture CH1 (GP26) and CH2 (GP27) interleaved" OFF)
option(PI_PICO_SCOPE_LOGIC_ANALYSER "Digital lane mode instead of the analog trace" OFF)

if(PI_PICO_SCOPE_CONSTANT_COLOR_TEST)
    set(PI_PICO_SCOPE_SOURCES
        src/panel_constant_color.c
        src/bsp.c
    )
else()
    set(PI_PICO_SCOPE_SOURCES
        src/main.c
        src/bsp.c
        src/gfx.c
        src/panel_st7789.c
        src/panel_smoke.c
        src/scope.c
        src/wave.c
        src/ui.c
    )
endif()

add_executable(pi-pico-scope ${PI_PICO_SCOPE_SOURCES})

execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    OUTPUT_VARIABLE FW_GIT_SHA
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(NOT FW_GIT_SHA)
    set(FW_GIT_SHA "nogit")
endif()

target_compile_definitions(pi-pico-scope PRIVATE FW_VERSION="${FW_GIT_SHA}")

# Dual channel and logic analyser are opt-in: with both off the firmware is the
# original single-channel analog scope, byte-for-byte identical to before.
if(PI_PICO_SCOPE_DUAL_ADC)
    target_compile_definitions(pi-pico-scope PRIVATE ADC_CHANNELS=2)
endif()
if(PI_PICO_SCOPE_LOGIC_ANALYSER)
    target_compile_definitions(pi-pico-scope PRIVATE LOGIC_ANALYSER=1)
endif()
if(PI_PICO_SCOPE_PANEL_PROBE)
    target_compile_definitions(pi-pico-scope PRIVATE PI_PICO_SCOPE_PANEL_PROBE=1)
endif()
if(PI_PICO_SCOPE_SLOW_SPI_PROBE)
    target_compile_definitions(pi-pico-scope PRIVATE LCD_USE_SOFT_SPI=1 LCD_SOFT_SPI_HALF_PERIOD_US=2)
endif()
if(PI_PICO_SCOPE_REFERENCE_SPI_PROBE)
    target_compile_definitions(pi-pico-scope PRIVATE LCD_SPI_BAUD_HZ=20000000u LCD_USE_SOFT_SPI=0)
endif()
if(PI_PICO_SCOPE_CONSTANT_COLOR_TEST)
    target_compile_definitions(pi-pico-scope PRIVATE LCD_SPI_BAUD_HZ=20000000u LCD_USE_SOFT_SPI=0)
endif()

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
)

pico_enable_stdio_usb(pi-pico-scope 1)
pico_enable_stdio_uart(pi-pico-scope 0)

pico_add_extra_outputs(pi-pico-scope)
