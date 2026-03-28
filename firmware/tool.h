class Tool {
 public:
  void init(char *name);
  bool auth(char *serial);
  void complete();
  bool induct(char *inductor_serial, char *inductee_serial);

 private:
  bool waiting_auth;
  bool waiting_induct;

  void handleMqttMessage(char *payload, byte *payload, int length);
}
