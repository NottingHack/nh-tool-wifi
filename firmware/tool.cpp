extern "C" {
#include <cstdio>
#include <cstring>
#include "FreeRTOS.h"
#include "pico/stdlib.h"
#include "config.h"
}

#include "mfrc522.h"
#include "mqtt.h"
#include "events.h"

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

  clearUid();

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
  mqtt_message_t message;
  memset(&message, 0, sizeof(message));

  message.topic = topic_RESET;
  snprintf(message.payload, sizeof(message.payload), "BOOT");
  
  assert(xQueueSend(pub_queue, &message, portMAX_DELAY) == pdPASS);
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
  assert(xQueueSend(sub_queue, &topic_GRANT, portMAX_DELAY) == pdPASS);
  assert(xQueueSend(sub_queue, &topic_DENY, portMAX_DELAY) == pdPASS);
  assert(xQueueSend(sub_queue, &topic_ISUC, portMAX_DELAY) == pdPASS);
  assert(xQueueSend(sub_queue, &topic_IFAL, portMAX_DELAY) == pdPASS);

  // TODO: Signal connect/disconnect properly
  handleConnect();

  for (;;)
  {
    // Anything from MQTT?
    mqtt_message_t incoming;
    if (xQueueReceive(incoming_queue, &incoming, 10) == pdPASS)
    {
      printf("Tool: Incoming Message: Topic: %s Payload: %s\n", incoming.topic, incoming.payload);
      handleMqttMessage(incoming.topic, incoming.payload, strlen(incoming.payload));
    }

    // Anything from the reader?
    MFRC522::Uid card_id;
    if (xQueueReceive(uid_queue, &card_id, 10) == pdPASS)
    {
        printf("Tool: Seen Card: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\tSize: %d\tSAK: %x\n",
          card_id.uidByte[0],
          card_id.uidByte[1],
          card_id.uidByte[2],
          card_id.uidByte[3],
          card_id.uidByte[4],
          card_id.uidByte[5],
          card_id.uidByte[6],
          card_id.uidByte[7],
          card_id.uidByte[8],
          card_id.uidByte[9],
          card_id.size,
          card_id.sak
        );

        handleNewCard(card_id.uidByte);
    }
    
    // Any buttons?
    button_event_t button_event;
    if (xQueueReceive(button_queue, &button_event, 10) == pdPASS)
    {
      // Signoff button was just pressed
      if (button_event.change_mask & button_event.button_state & (1 << PIN_BUTTON_INDUCT))
      {
        printf("Tool: Induct Pressed\n");
        handleInduct();
      }

      // Induct button was just pressed
      if (button_event.change_mask & button_event.button_state & (1 << PIN_BUTTON_SIGNOFF))
      {
        printf("Tool: Sign Off Pressed\n");
        handleSignoff();
      }
    }
  }
}

void Tool::handleNewCard(uint8_t uid[10])
{
  mqtt_message_t message;
  memset(&message, 0, sizeof(message));
  
  if (state == DEV_IDLE)
  {
    state = DEV_AUTH_WAIT;

    // Record UID
    memcpy(current_uid, uid, sizeof(uint8_t) * 10);

    // Send AUTH
    message.topic = topic_AUTH;
    snprintf(
      message.payload,
      sizeof(message.payload),
      "%02x%02x%02x%02x%02x%02x%02x%02x", 
      uid[0],
      uid[1],
      uid[2],
      uid[3],
      uid[4],
      uid[5],
      uid[6],
      uid[7]
    );
    assert(xQueueSend(pub_queue, &message, portMAX_DELAY) == pdPASS);

    // Start timeout
  }
  else if (state == DEV_INDUCT)
  {
    state = DEV_INDUCT_WAIT;

    // Send INDUCT
    message.topic = topic_INDUCT;
    snprintf(
      message.payload,
      sizeof(message.payload),
      "%02x%02x%02x%02x%02x%02x%02x%02x:%02x%02x%02x%02x%02x%02x%02x%02x",
      current_uid[0],
      current_uid[1],
      current_uid[2],
      current_uid[3],
      current_uid[4],
      current_uid[5],
      current_uid[6],
      current_uid[7],
      uid[0],
      uid[1],
      uid[2],
      uid[3],
      uid[4],
      uid[5],
      uid[6],
      uid[7]
    );
    assert(xQueueSend(pub_queue, &message, portMAX_DELAY) == pdPASS);

    // Start timeout
  }
  else
  {
    printf("Tool: Unexpected card in state %d\n", state);
  }
}

void Tool::handleCardRemoved()
{
  if (state == DEV_ACTIVE)
  {
    // Start timeout
  }
  else
  {
    printf("Tool: Card Removed from unexpected state %d\n", state);
  }
}

void Tool::handleInduct()
{
  if (state == DEV_ACTIVE)
  {
    if (is_inductor)
    {
      state = DEV_INDUCT;
    }
  }
  else
  {
    printf("Tool: Unexpected Induct in state %d\n", state);
  }
}

void Tool::handleSignoff()
{
  if (state == DEV_ACTIVE)
  {
    state = DEV_IDLE;

    // Send COMPLETE
    mqtt_message_t message;
    memset(&message, 0, sizeof(message));

    // TODO: Track active time
    message.topic = topic_COMPLETE;
    snprintf(message.payload, sizeof(message.payload), "<TOOL ACTIVE TIME>");
  
    assert(xQueueSend(pub_queue, &message, portMAX_DELAY) == pdPASS);
  }
  else if (state == DEV_INDUCT)
  {
    state = DEV_IDLE;
  }
  else
  {
    printf("Tool: Unexpected Sign Off from %d\n", state);
  }
}

void Tool::handleConnect()
{
  if (state == DEV_NO_CONN)
  {
    state = DEV_IDLE;

    clearUid();

    // Send RESET
    mqtt_message_t message;
    memset(&message, 0, sizeof(message));

    // TODO: Figure out if we're active or idle
    message.topic = topic_RESET;
    snprintf(message.payload, sizeof(message.payload), "IDLE");
  
    assert(xQueueSend(pub_queue, &message, portMAX_DELAY) == pdPASS);
  }
  else
  {
    printf("Tool: Connect from unexpected state %d\n", state);
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

void Tool::handleMqttMessage(const char *topic, const char *payload, int length)
{
  if (topic == topic_GRANT)
  {
    if (state == DEV_AUTH_WAIT)
    {
      state = DEV_ACTIVE;

      if (payload[0] == 'I')
      {
        printf("Tool: Inductor Signed On\n");
        is_inductor = true;
      }
      else if (payload[0] == 'U')
      {
        printf("Tool: User Signed On\n");
        is_inductor = false;
      }
      else if (payload[0] == 'M')
      {
        printf("Tool: Maintainer Signed On\n");
        is_inductor = false;
      }
      else
      {
        printf("Tool: Unexpected User Type \"%c\" Signed On\n", payload[0]);
        is_inductor = false;
      }

      printf("%s\n", payload + 1);
    }
    else
    {
      printf("Tool: GRANT from unexpected state %d\n", state);
    }
  }
  else if (topic == topic_DENY)
  {
    if (state == DEV_AUTH_WAIT)
    {
      state = DEV_IDLE;

      clearUid();

      // What if not connected?
    }
    else
    {
      printf("Tool: DENY from unexpected state %d\n", state);
    }
  }
  else if (topic == topic_ISUC)
  {
    if (state == DEV_INDUCT_WAIT)
    {
      state = DEV_IDLE;
    }
    else
    {
      printf("Tool: ISUC from unexpected state %d\n", state);
    }
  }
  else if (topic == topic_IFAL)
  {
    if (state == DEV_INDUCT_WAIT)
    {
      state = DEV_INDUCT;

      printf("Tool: Induct Failed\n%s\n", payload);
    }
    else
    {
      printf("Tool: IFAL from unexpected state %d\n", state);
    }
  }
}

void Tool::clearUid()
{
    // Clear current UID
    memset(current_uid, 0, sizeof(uint8_t) * 10);

    // Not an inductor
    is_inductor = false;
}

void vToolDispatcher(void* in_params)
{
  Tool* tool = reinterpret_cast<Tool*>(in_params);
  tool->runTask();
}

