#pragma once

extern "C" {
  #include "FreeRTOS.h"
  #include "event_groups.h"
  #include "timers.h"
  #include "queue.h"
}

typedef struct
{
  const char *name;
  const char* base_topic;
  QueueHandle_t sub_queue;
  QueueHandle_t pub_queue;
  QueueHandle_t incoming_queue;
  QueueHandle_t uid_queue;
  QueueHandle_t led_queue;
  QueueHandle_t button_queue;
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

  //Tool(const char* name, Queuehandle_t led_queue);

  void init(tool_params_t params);

  // FreeRTOS task loop
  void runTask();

private:

  //bool auth(char *serial);
  //void complete();
  //bool induct(char *inductor_serial, char *inductee_serial);

  // RFID handlers
  void handleNewCard(uint8_t uid[10]);
  void handleCardRemoved();

  // Button handlers
  void handleInduct();
  void handleSignoff();

  // MQTT message handlers
  void handleConnect();
  void handleDisconnect();
  void handleMqttMessage(const char *topic, const char *payload, int length);

  // Utility
  void clearUid();

  // Parameters
  const char *name;

  QueueHandle_t sub_queue;
  QueueHandle_t pub_queue;
  QueueHandle_t incoming_queue;

  QueueHandle_t uid_queue;

  QueueHandle_t led_queue;
  QueueHandle_t button_queue;

  TimerHandle_t timeout_timer;

  // State
  ToolState state;

  // User's card
  uint8_t current_uid[10];
  bool is_inductor;
};

// Dispatcher to invoke the tool's runTask method
void vToolDispatcher(void* in_params);

