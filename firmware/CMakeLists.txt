cmake_minimum_required(VERSION 3.13...3.27)

include(pico_sdk_import.cmake)

project(nh-tool-wifi)

pico_sdk_init()

add_executable(nh-tool-wifi
    nh-tool-wifi.c
)

# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(nh-tool-wifi pico_stdlib)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(nh-tool-wifi)
