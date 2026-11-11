#pragma once

extern "C" {
  #include "FreeRTOS.h"
  #include "semphr.h"
  #include "timers.h"
  #include "queue.h"
}

#include "events.h"

typedef struct
{
  const char *name;
  const char* base_topic;
  QueueHandle_t sub_queue;
  QueueHandle_t pub_queue;
  QueueHandle_t incoming_queue;
  QueueHandle_t uid_queue;
  QueueHandle_t led_queue;
  QueueHandle_t relay_queue;
  QueueHandle_t button_queue;
  SemaphoreHandle_t connection_state_semaphore;
} tool_params_t;

class Tool
{
public:
  enum ToolState
  {
    DEV_NO_CONN = 0,
    DEV_IDLE,
    DEV_AUTH_WAIT,
    DEV_ACTIVE,
    DEV_INDUCT,
    DEV_INDUCT_WAIT,
    NUM,
  };

  // MQTT Topic strings
 
  // Publish
  const char* topic_AUTH     = nullptr;
  const char* topic_COMPLETE = nullptr;
  const char* topic_INDUCT   = nullptr;
  const char* topic_RESET    = nullptr;
  
  // Subscribe
  const char* topic_GRANT    = nullptr;
  const char* topic_DENY     = nullptr;
  const char* topic_ISUC     = nullptr;
  const char* topic_IFAL     = nullptr;

  void init(tool_params_t params);

  // FreeRTOS task loop
  void runTask();

private:

  // RFID handlers
  void handleNewCard(uint8_t uid[10]);

  // Button handlers
  void handleInduct();
  void handleSignoff();

  // MQTT message handlers
  void handleConnect(const char* payload);
  void handleDisconnect();
  void handleMqttMessage(const char *topic, const char *payload, int length);

  // LEDs and Relay
  void ledOn(int index);
  void ledOff(int index);
  void ledSet(int index, bool state);
  void relayOn();
  void relayOff();

  // Timeout
  void setTimeout(int timeout);
  void clearTimeout();
  void restartTimeout();
  void handleTimeout();

  static void timeoutHandler(TimerHandle_t timer);

  // Session management
  void clearUid();
  uint64_t getSessionTimeInMilliseconds();
  int getSessionHours();
  int getSessionMinutes();
  int getSessionSeconds();

  // Parameters
  const char *name;

  QueueHandle_t sub_queue;
  QueueHandle_t pub_queue;
  QueueHandle_t incoming_queue;

  QueueHandle_t uid_queue;

  QueueHandle_t led_queue;
  QueueHandle_t relay_queue;
  QueueHandle_t button_queue;

  SemaphoreHandle_t timeout_semaphore;
  TimerHandle_t timeout_timer;

  SemaphoreHandle_t connection_state_semaphore;

  // State
  ToolState state;
  led_event_t led_state;

  // Session data
  uint8_t current_uid[10];
  bool is_inductor;
  TickType_t session_start;
};

// Dispatcher to invoke the tool's runTask method
void vToolDispatcher(void* in_params);

