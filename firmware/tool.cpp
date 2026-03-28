
void Tool::init(char *name) {
  waiting_auth = false;
  waiting_induct = false;
}

bool Tool::auth(char *serial) {
  waiting_auth = true;
}

void Tool::complete() {

}

bool Tool::induct(char *inductor_serial, char *inductee_serial) {
  waiting_induct = true;
}

void Tool::handleMqttMessage(char *topic, byte *payload, int length) {
  
}
