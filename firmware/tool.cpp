extern "C" {
#include <cstdio>
#include "FreeRTOS.h"
#include "pico/stdlib.h"
#include "config.h"
}

#include "tool.h"

void Tool::init(const char *name) {
  state = DEV_NO_CONN;
  // Send RESET
}

/*
bool Tool::auth(char *serial) {
  waiting_auth = true;
  return false;
}
*/

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
    printf("Connect from unexpected state...\n");
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
