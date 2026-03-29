extern "C" {
#include <stdio.h>
#include "FreeRTOS.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "lwip/apps/mqtt.h"
#include "pico/stdlib.h"
#include "config.h"
#include "tool.h"
#include "task.h"
}

void init_pins() {
  gpio_init(PIN_LED_INDUCT);
  gpio_set_dir(PIN_LED_INDUCT, GPIO_OUT);
  gpio_init(PIN_LED_TOOL);
  gpio_set_dir(PIN_LED_TOOL, GPIO_OUT);
  gpio_init(PIN_LED_STATE);
  gpio_set_dir(PIN_LED_STATE, GPIO_OUT);

  gpio_init(PIN_RELAY_SET);
  gpio_set_dir(PIN_RELAY_SET, GPIO_OUT);
  gpio_init(PIN_RELAY_RESET);
  gpio_set_dir(PIN_RELAY_RESET, GPIO_OUT);

  gpio_init(PIN_BUTTON_INDUCT);
  gpio_set_dir(PIN_BUTTON_INDUCT, GPIO_IN);
  gpio_init(PIN_BUTTON_SIGNOFF);
  gpio_set_dir(PIN_BUTTON_SIGNOFF, GPIO_IN);
}

void vBlinkTask(void *params) {
   for (;;) {
      gpio_put(PIN_LED_TOOL, 1);
      vTaskDelay(250);
      gpio_put(PIN_LED_TOOL, 0);
      vTaskDelay(250);
   }
}

int main() {
    stdio_init_all();
    init_pins();

    xTaskCreate(vBlinkTask, "Blink Task", 128, NULL, 1, NULL);
    vTaskStartScheduler();

    Tool *tool;
    tool->init("Bandsaw");

    return 0;
}
