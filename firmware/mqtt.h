#pragma once

extern "C" {
#include "FreeRTOS.h"
#include "lwip/tcp.h"
#include "queue.h"
}

#define MESSAGE_LEN 64

typedef struct
{
  const char* topic;              // Allocated from a pool of topic strings
  char payload[MESSAGE_LEN + 1];  // Pass the message around in a buffer
} mqtt_message_t;

typedef struct
{
  const char* device_name;

  const char* ssid;
  const char* password;

  ip_addr_t ip;

  ip_addr_t server;
  uint16_t port;

  // Messages written to the queue will be published
  xQueueHandle pub_queue;

  // Topic names written to the queue will be subscribed to
  xQueueHandle sub_queue;

  // Incoming messages to subscribed topics will be written here
  xQueueHandle incoming_queue;
} mqtt_task_params_t;

void vMqttTask(void* params);

