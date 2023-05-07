# Hello world executable
add_executable(canairy_simple_data simple_data.c)

# Link dependencies
target_link_libraries(canairy_simple_data blecon-modem blecon-modem_pico pico_stdlib cmsis_core)

# Direct stdio to USB
pico_enable_stdio_usb(canairy_simple_data 1)
pico_enable_stdio_uart(canairy_simple_data 0)

# Create Pico UF2, map, hex, etc. files
pico_add_extra_outputs(canairy_simple_data)