cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico2 CACHE STRING "Board type")
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(osc440 C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(osc440
    src/main.cpp
    src/io/i2s_out.cpp
    src/system/clocks.cpp
    src/dsp/buffer.cpp
    src/dsp/interpolate.cpp
    src/synth/saw_osc.cpp
    src/synth/saw_tables.cpp
    src/synth/adsr.cpp
    src/synth/wavetable_osc.cpp
    src/synth/wavetable_data.cpp
    src/synth/wavetable_plaits.cpp
    src/synth/oscillator.cpp
    src/synth/dadsr.cpp
    src/synth/poly_group.cpp
    src/synth/mseg.cpp
    src/synth/lfo.cpp
    src/synth/triple_wave_group.cpp
    src/engine/voice_allocator.cpp
    src/engine/voice_engine.cpp
)

target_include_directories(osc440 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)

pico_generate_pio_header(osc440 ${CMAKE_CURRENT_LIST_DIR}/src/io/i2s_out.pio)

target_compile_options(osc440 PRIVATE
    -O2 -Wall -Wextra
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti>
)

option(OSC440_OVERCLOCK "Run at 307.2 MHz instead of 153.6 MHz (untested on hardware)" OFF)
if(OSC440_OVERCLOCK)
    target_compile_definitions(osc440 PRIVATE OSC440_OVERCLOCK=1)
endif()

target_link_libraries(osc440
    pico_stdlib
    pico_multicore
    hardware_pio
    hardware_dma
    hardware_clocks
    hardware_vreg
)

# Status prints go to UART0 (GP0/GP1), so nothing in the audio path waits on USB
pico_enable_stdio_uart(osc440 1)
pico_enable_stdio_usb(osc440 0)

pico_add_extra_outputs(osc440)
