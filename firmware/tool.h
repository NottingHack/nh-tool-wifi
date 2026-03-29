#ifndef TOOL_H
#define TOOL_H

class Tool {
 public:
  void init(const char *name);
  bool auth(char *serial);
  void complete();
  bool induct(char *inductor_serial, char *inductee_serial);

 private:
  bool waiting_auth;
  bool waiting_induct;

  void handleMqttMessage(char *topic, char *payload, int length);
};

#endif
