#include "tool.h"

void Tool::init(const char *name) {
  waiting_auth = false;
  waiting_induct = false;
}

bool Tool::auth(char *serial) {
  waiting_auth = true;
  return false;
}

void Tool::complete() {

}

bool Tool::induct(char *inductor_serial, char *inductee_serial) {
  waiting_induct = true;
  return false;
}

void Tool::handleMqttMessage(char *topic, char *payload, int length) {

}
