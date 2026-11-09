extern "C" {
#include <cstdio>
#include <cstring>
#include "FreeRTOS.h"
#include "pico/stdlib.h"
#include "config.h"
}

#include "mfrc522.h"
#include "mqtt.h"

#include "tool.h"

const char* build_topic(const char* prefix, const char* device, const char* suffix)
{
  const size_t length = strlen(prefix) + strlen(device) + strlen(suffix) + 2 /* slashes */ + 1 /* nul */;
  char* result = reinterpret_cast<char*>(malloc(sizeof(char) * length));
  if (result)
  {
    snprintf(result, length, "%s/%s/%s", prefix, device, suffix);
  }
  return result;
}

void Tool::init(tool_params_t params) {
  // Copy the relevant parameters
  name = params.name;

  sub_queue = params.sub_queue;
  pub_queue = params.pub_queue;
  incoming_queue = params.incoming_queue;

  uid_queue = params.uid_queue;

  led_queue = params.led_queue;
  button_queue = params.button_queue;

  // Build topic string pool
  const char* topic_base = params.base_topic;

  // Outgoing
  topic_AUTH =     build_topic(params.base_topic, name, "AUTH");
  topic_COMPLETE = build_topic(params.base_topic, name, "COMPLETE");
  topic_INDUCT =   build_topic(params.base_topic, name, "INDUCT");
  topic_RESET =    build_topic(params.base_topic, name, "RESET");

  // Incoming
  topic_GRANT =    build_topic(params.base_topic, name, "GRANT");
  topic_DENY =     build_topic(params.base_topic, name, "DENY");
  topic_ISUC =     build_topic(params.base_topic, name, "ISUC");
  topic_IFAL =     build_topic(params.base_topic, name, "IFAL");

  state = DEV_NO_CONN;
  // Send RESET
}

/*
bool Tool::auth(char *serial) {
  waiting_auth = true;
  return false;
}
*/

void Tool::runTask()
{
  vTaskDelay(3000);

  // Subscribe to the relevant topics
  printf("Tool: Request subscribe %s [%x]\n", topic_GRANT, topic_GRANT);
  assert(xQueueSend(sub_queue, &topic_GRANT, portMAX_DELAY) == pdPASS);
  printf("Tool: Request subscribe %s [%x]\n", topic_DENY, topic_DENY);
  assert(xQueueSend(sub_queue, &topic_DENY, portMAX_DELAY) == pdPASS);
  printf("Tool: Request subscribe %s [%x]\n", topic_ISUC, topic_ISUC);
  assert(xQueueSend(sub_queue, &topic_ISUC, portMAX_DELAY) == pdPASS);
  printf("Tool: Request subscribe %s [%x]\n", topic_IFAL, topic_IFAL);
  assert(xQueueSend(sub_queue, &topic_IFAL, portMAX_DELAY) == pdPASS);

  for (;;)
  {
    // Anything from MQTT?
    mqtt_message_t incoming;
    if (xQueueReceive(incoming_queue, &incoming, 10) == pdPASS)
    {
      printf("Tool: Incoming Message: Topic: %s Payload: %s\n", incoming.topic, incoming.message);
    }

    // Anything from the reader?
    MFRC522::Uid card_id;
    if (xQueueReceive(uid_queue, &card_id, 10) == pdPASS)
    {
      
    }
    
    // Any buttons?
    /*
    button_event_t button_event;
    if (xQueueReceive(button_queue, &button_event, 10) == pdPASS)
    {

    }
    */
  }
}

void Tool::handleNewCard(uint8_t uid[10])
{
  if (state == DEV_IDLE)
  {
    state = DEV_AUTH_WAIT;

    // Send AUTH
    // Start timeout
  }
  else if (state == DEV_INDUCT)
  {
    state = DEV_INDUCT_WAIT;

    // Send INDUCT
    // Start timeout
  }
}

void Tool::handleCardRemoved()
{
  if (state == DEV_ACTIVE)
  {
    // Start timeout
  }
}

void Tool::handleInduct()
{
  if (state == DEV_ACTIVE)
  {
    // Check user is an inductor
    state = DEV_INDUCT;
  }
}

void Tool::handleSignoff()
{
  if (state == DEV_ACTIVE)
  {
    state = DEV_IDLE;
    // Send COMPLETE
  }
  else if (state == DEV_INDUCT)
  {
    state = DEV_IDLE;
  }
}

void Tool::handleConnect()
{
  if (state == DEV_NO_CONN)
  {
    state = DEV_IDLE;
    // Send RESET
  }
  else
  {
    printf("Tool: Connect from unexpected state...\n");
  }
}

void Tool::handleDisconnect()
{
  if (state == DEV_IDLE)
  {
    state = DEV_NO_CONN;
  }
  // Other states ignore disconnects until signoff
}

void Tool::handleMqttMessage(char *topic, char *payload, int length)
{
#if 0
  if (/*GRANT*/)
  {
    if (state == DEV_AUTH_WAIT)
    {
      state = DEV_ACTIVE;
    }
  }
  else if (/*DENY*/)
  {
    if (state == DEV_AUTH_WAIT)
    {
      state = DEV_IDLE;
      // What if not connected?
    }
  }
  else if (/*ISUC*/)
  {
    if (state == DEV_INDUCT_WAIT)
    {
      state = DEV_IDLE;
    }
  }
  else if (/*IFAL*/)
  {
    if (state == DEV_INDUCT_WAIT)
    {
      // Display reason
    }
  }
#endif
}

void vToolDispatcher(void* in_params)
{
  Tool* tool = reinterpret_cast<Tool*>(in_params);
  tool->runTask();
}

