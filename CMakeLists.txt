cmake_minimum_required(VERSION 3.16)

include(pico-sdk/pico_sdk_init.cmake)

project(DeskDoorbell)

pico_sdk_init()

### Adafruit_NeoPixel library
add_library(pico_neopixel INTERFACE)

pico_generate_pio_header(pico_neopixel
    ${CMAKE_SOURCE_DIR}/pi-pico-adafruit-neopixels/pico_neopixels/ws2812byte.pio
)

target_sources(pico_neopixel INTERFACE
    ${CMAKE_SOURCE_DIR}/pi-pico-adafruit-neopixels/pico_neopixels/Adafruit_NeoPixel.cpp
)

target_include_directories(pico_neopixel INTERFACE
    ${CMAKE_SOURCE_DIR}/pi-pico-adafruit-neopixels/pico_neopixels/include
)

# Pull in pico libraries that we need
target_link_libraries(pico_neopixel INTERFACE pico_stdlib hardware_pio pico_malloc pico_mem_ops)

### Actual project
add_executable(DeskDoorbell)
target_sources(DeskDoorbell PRIVATE
    ${CMAKE_SOURCE_DIR}/main.cpp
)

# Link with the pico stdlib
target_link_libraries(DeskDoorbell
    pico_stdlib
    hardware_adc
    hardware_pio
    pico_neopixel
)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(DeskDoorbell)

# Enable USB serial
pico_enable_stdio_usb(DeskDoorbell 1)
pico_enable_stdio_uart(DeskDoorbell 0)
