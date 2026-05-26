#pragma once

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

  //Tool(const char* name, Queuehandle_t led_queue);

  void init(const char *name);
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
  void handleMqttMessage(char *topic, char *payload, int length);

  ToolState state;
};
