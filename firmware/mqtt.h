#pragma once

extern "C" {
#include "FreeRTOS.h"
#include "queue.h"
}

// String pool for topic names
//
// Publish
extern char* topic_AUTH;
extern char* topic_COMPLETE;
extern char* topic_INDUCT;
extern char* topic_RESET;
// Subscribe
extern char* topic_GRANT;
extern char* topic_DENY;
extern char* topic_ISUC;
extern char* topic_IFAL;

typedef struct
{
  char* topic;      // Allocated from a pool of topic strings
  char message[0];  // Allocated inline with the struct
} mqtt_message_t;

typedef struct
{
  const char* device_name;

  const char* ssid;
  const char* password;

  ip_addr_t server;
  uint16_t port;

  xQueueHandle pub_queue;
  xQueueHandle sub_queue;
} mqtt_task_params_t;

void vMqttTask(void* params);

