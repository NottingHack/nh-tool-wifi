extern "C" {
#include <stdio.h>
#include "FreeRTOS.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "lwip/apps/mqtt.h"
#include "pico/stdlib.h"
#include "config.h"
#include "task.h"
#include "queue.h"
}

#include "tool.h"

#include "events.h"
#include "mfrc522.h"
#include "mqtt.h"
#include "settings.h"

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
  xQueueHandle uid_queue;
} rfid_params_t;

void vRfidTask(void* params)
{
  if (!params)
  {
    return;
  }

  rfid_params_t rfid_params = *reinterpret_cast<rfid_params_t*>(params);

  MFRC522 reader;

  reader.PCD_Init();
  //printf("RFID: Reader Self Test: %s\n", reader.PCD_PerformSelfTest() ? "PASS" : "FAIL!");
  reader.PCD_DumpVersionToSerial();	

  for (;;)
  {
    if (reader.PICC_IsNewCardPresent())
    {
      if (reader.PICC_ReadCardSerial())
      {
        /*
        printf("RFID: Card Found:\n\tSize: %d\n\tUID: %x %x %x %x %x %x %x %x %x %x\n\tSAK: %x\n",
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
        */

        if (xQueueSend(rfid_params.uid_queue, &reader.uid, 10) != pdPASS)
        {
          printf("RFID: Couldn't enqueue UID...\n");
        }
      }
      vTaskDelay(300);
    }

    vTaskDelay(0);
  }
}

int main()
{
  // Board initialistion
  stdio_init_all();
  stdio_usb_init();
  init_pins();

  // Load settings from FLASH
  init_settings();

  // Setup button/LED tasks
  QueueHandle_t button_queue = xQueueCreate(1, sizeof(button_event_t));
  QueueHandle_t led_queue = xQueueCreate(1, sizeof(button_event_t));

  xTaskCreate(vButtonTask, "Button Task", 128, reinterpret_cast<void*>(&button_queue), 1, nullptr);
  xTaskCreate(vLEDTask, "LED Task", 128, reinterpret_cast<void*>(&led_queue), 1, nullptr);

  // Setup RFID task
  QueueHandle_t uid_queue = xQueueCreate(1, sizeof(MFRC522::Uid));
  rfid_params_t rfid_params = {
    .uid_queue = uid_queue,
  };

  xTaskCreate(vRfidTask, "RFID Task", 1024, reinterpret_cast<void*>(&rfid_params), 1, nullptr);

  // Setup MQTT / WiFi task
  QueueHandle_t pub_queue = xQueueCreate(4, sizeof(mqtt_message_t));
  QueueHandle_t sub_queue = xQueueCreate(4, sizeof(char*));
  QueueHandle_t incoming_queue = xQueueCreate(4, sizeof(mqtt_message_t));

  const settings_t* settings = get_settings();
  mqtt_task_params_t mqtt_params = {
    .device_name = settings->device_name,
    .ssid = settings->ssid,
    .password = settings->password,
    .ip = settings->ip_addr,
    .server = settings->server_addr,
    .port = 1883,
    .pub_queue = pub_queue,
    .sub_queue = sub_queue,
    .incoming_queue = incoming_queue,
  };

  xTaskCreate(vMqttTask, "MQTT Task", 1024, &mqtt_params, 2, nullptr);

  // Setup the tool task
  tool_params_t tool_params = {
    .name = settings->device_name,
    .base_topic = settings->base_topic,
    .sub_queue = sub_queue,
    .pub_queue = pub_queue,
    .incoming_queue = incoming_queue,
    .uid_queue = uid_queue,
    .led_queue = led_queue,
    .button_queue = button_queue,
  };

  Tool tool;
  tool.init(tool_params);

  xTaskCreate(vToolDispatcher, settings->device_name, 1024, &tool, 1, nullptr);

  vTaskStartScheduler();

  return 0;
}

