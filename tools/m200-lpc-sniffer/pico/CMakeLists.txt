cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
project(m200_lpc_sniffer C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(m200_lpc_sniffer main.c)
pico_generate_pio_header(m200_lpc_sniffer
    ${CMAKE_CURRENT_LIST_DIR}/lpc_capture.pio)
target_include_directories(m200_lpc_sniffer PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../common)
target_link_libraries(m200_lpc_sniffer PRIVATE
    pico_stdlib pico_stdio_usb hardware_clocks hardware_dma hardware_pio)
pico_enable_stdio_usb(m200_lpc_sniffer 1)
pico_enable_stdio_uart(m200_lpc_sniffer 0)
pico_add_extra_outputs(m200_lpc_sniffer)
