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
#include "queue.h"
}

#include "mfrc522.h"
#include "mqtt.h"

void init_pins()
{
  for (int pin_config_index = 0; pin_config_index < num_pin_configs; ++pin_config_index)
  {
    const pin_config_t* pin_config = &pin_configs[pin_config_index];
    gpio_init(pin_config->pin_number);
    gpio_set_dir(pin_config->pin_number, pin_config->pin_dir);
    if (pin_config->pin_dir == GPIO_OUT)
    {
      gpio_put(pin_config->pin_number, pin_config->initial_value);
    }
  }
}

void vBlinkTask(void *params)
{
  for (;;)
  {
    gpio_put(PIN_LED_TOOL, 1);
    vTaskDelay(250);
    gpio_put(PIN_LED_TOOL, 0);
    vTaskDelay(250);
  }
}

typedef struct
{
  uint32_t led_state;
} led_event_t;

void vLEDTask(void* params)
{
  QueueHandle_t queue = *reinterpret_cast<QueueHandle_t*>(params);

  gpio_put(PIN_LED_INDUCT, 0);
  gpio_put(PIN_LED_TOOL, 0);
  gpio_put(PIN_LED_STATE, 0);

  if (queue)
  {
    for (;;)
    {
      led_event_t led_event;
      if (xQueueReceive(queue, &led_event, portMAX_DELAY) == pdPASS)
      {
        gpio_put(PIN_LED_INDUCT, led_event.led_state & (1 << PIN_LED_INDUCT));
        gpio_put(PIN_LED_TOOL, led_event.led_state & (1 << PIN_LED_TOOL));
        gpio_put(PIN_LED_STATE, led_event.led_state & (1 << PIN_LED_STATE));
      }
    }
  }
}

typedef struct
{
  uint32_t button_state;
  uint32_t change_mask;
} button_event_t;

void vButtonTask(void* params)
{
  QueueHandle_t queue = *reinterpret_cast<QueueHandle_t*>(params);

  if (queue)
  {
    uint32_t button_mask = 0x00;
    for (;;)
    {
      uint32_t new_mask = 0x00;
      new_mask |= gpio_get(PIN_BUTTON_INDUCT) << PIN_BUTTON_INDUCT;
      new_mask |= gpio_get(PIN_BUTTON_SIGNOFF) << PIN_BUTTON_SIGNOFF;

      if (new_mask != button_mask)
      {
        button_event_t event;

        event.button_state = new_mask;
        event.change_mask = new_mask ^ button_mask;

        button_mask = new_mask;

        if (xQueueSend(queue, &event, 10) != pdPASS)
        {
          // Something went wrong
        }
      }

      vTaskDelay(0);
    }
  }
}

typedef struct
{
  xQueueHandle leds;
  xQueueHandle buttons;
} test_params_t;

void vTestTask(void* params)
{
  if (!params)
  {
    return;
  }

  test_params_t test_params = *reinterpret_cast<test_params_t*>(params);

  for (;;)
  {
    button_event_t button_event;

    printf("Tick\n");

    if (xQueueReceive(test_params.buttons, &button_event, portMAX_DELAY) == pdPASS)
    {
      led_event_t led_event;
      led_event.led_state = button_event.button_state >> 2;

      if (xQueueSend(test_params.leds, &led_event, 10) != pdPASS)
      {
        // Something went wrong
      }
    }
  }
}

void vRfidTask(void* params)
{
  if (!params)
  {
    return;
  }

  test_params_t test_params = *reinterpret_cast<test_params_t*>(params);

  led_event_t led_event;
  led_event.led_state = -1;

  MFRC522 reader;

  reader.PCD_Init();
  //printf("RFID Reader Self Test: %s\n", reader.PCD_PerformSelfTest() ? "PASS" : "FAIL!");
  reader.PCD_DumpVersionToSerial();	

  vTaskDelay(3000);

  printf("Ready\n");

  for (;;)
  {
    if (reader.PICC_IsNewCardPresent())
    {
      printf("\nCard Present!\n");

      led_event.led_state |= 1 << PIN_LED_INDUCT;

      if (xQueueSend(test_params.leds, &led_event, 10) != pdPASS)
      {
        // Something went wrong
      }

      if (reader.PICC_ReadCardSerial())
      {
        led_event.led_state |= 1 << PIN_LED_TOOL;

        if (xQueueSend(test_params.leds, &led_event, 10) != pdPASS)
        {
          // Something went wrong
        }

        printf("Card Found:\n\tSize: %d\n\tUID: %x %x %x %x %x %x %x %x %x %x\n\tSAK: %x\n",
          reader.uid.size,
          reader.uid.uidByte[0],
          reader.uid.uidByte[1],
          reader.uid.uidByte[2],
          reader.uid.uidByte[3],
          reader.uid.uidByte[4],
          reader.uid.uidByte[5],
          reader.uid.uidByte[6],
          reader.uid.uidByte[7],
          reader.uid.uidByte[8],
          reader.uid.uidByte[9],
          reader.uid.sak
        );
      }
      vTaskDelay(300);
    }

    led_event.led_state &= ~((1 << PIN_LED_TOOL) | (1 << PIN_LED_INDUCT));
    led_event.led_state ^= ((1 << PIN_LED_STATE));

    if (xQueueSend(test_params.leds, &led_event, 10) != pdPASS)
    {
      // Something went wrong
    }

    vTaskDelay(0);
  }
}

int main()
{
  stdio_init_all();
  init_pins();

  QueueHandle_t button_queue = xQueueCreate(1, sizeof(button_event_t));
  QueueHandle_t led_queue = xQueueCreate(1, sizeof(button_event_t));

  //xTaskCreate(vBlinkTask, "Blink Task", 128, nullptr, 1, nullptr);
  xTaskCreate(vButtonTask, "Button Task", 128, reinterpret_cast<void*>(&button_queue), 1, nullptr);
  xTaskCreate(vLEDTask, "LED Task", 128, reinterpret_cast<void*>(&led_queue), 1, nullptr);

  test_params_t test_params;
  test_params.leds = led_queue;
  test_params.buttons = button_queue;

  //xTaskCreate(vTestTask, "TEST Task", 128, reinterpret_cast<void*>(&test_params), 1, nullptr);
  xTaskCreate(vRfidTask, "RFID Task", 1024, reinterpret_cast<void*>(&test_params), 1, nullptr);

  QueueHandle_t sub_queue = xQueueCreate(4, sizeof(mqtt_message_t*));
  QueueHandle_t pub_queue = xQueueCreate(4, sizeof(mqtt_message_t*));

  mqtt_task_params_t mqtt_params = {
    .device_name = "newtool",
    .ssid = "CHANGE_THIS",
    .password = "CHANGE_THIS_TOO",
    .port = 1883,
    .pub_queue = pub_queue,
    .sub_queue = sub_queue,
  };
  ipaddr_aton("192.168.0.59", &mqtt_params.server);

  xTaskCreate(vMqttTask, "MQTT Task", 1024, &mqtt_params, 2, nullptr);

  Tool tool;
  tool.init("Bandsaw");

  vTaskStartScheduler();

  return 0;
}

