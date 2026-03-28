cmake_minimum_required(VERSION 3.13...3.27)

set(PICO_BOARD pico_w)

include(libs/pico-sdk/pico_sdk_init.cmake)

project(nh-tool-wifi)

pico_sdk_init()

set(FREERTOS_KERNEL_PATH ${CMAKE_CURRENT_SOURCE_DIR}/libs/FreeRTOS-Kernel)
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)
include(libs/coreMQTT/mqttFilePaths.cmake)

include_directories(
  ${FREERTOS_KERNEL_PATH}/include
  ${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/include
  ${MQTT_INCLUDE_PUBLIC_DIRS}
  .
)

add_executable(nh-tool-wifi
    nh-tool-wifi.c
)

target_link_libraries(nh-tool-wifi pico_stdlib FreeRTOS-Kernel FreeRTOS-Kernel-Heap4)

pico_add_extra_outputs(nh-tool-wifi)

