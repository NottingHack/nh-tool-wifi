extern "C" {
#include <cstdio>
#include <cstring>
#include "FreeRTOS.h"
#include "pico/stdlib.h"
#include "config.h"
}

#include "config.h"
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

void Tool::timeoutHandler(TimerHandle_t timer)
{
  if (Tool* tool = (Tool*)pvTimerGetTimerID(timer))
  {
    // Signal timeout to the tool
    if (xSemaphoreGive(tool->timeout_semaphore) != pdTRUE)
    {
      assert(0 && "Tool: Unexpected double timeout!");
    }
  }
}

void Tool::init(tool_params_t params) {
  // Copy the relevant parameters
  name = params.name;

  sub_queue = params.sub_queue;
  pub_queue = params.pub_queue;
  incoming_queue = params.incoming_queue;

  uid_queue = params.uid_queue;

  led_queue = params.led_queue;
  relay_queue = params.relay_queue;
  button_queue = params.button_queue;

  connection_state_semaphore = params.connection_state_semaphore;

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

  // Create timers
  timeout_timer = xTimerCreate("timeout", pdMS_TO_TICKS(15000), pdFALSE, this, &Tool::timeoutHandler);
  timeout_semaphore = xSemaphoreCreateBinary();
}

void Tool::runTask()
{
  int last_session_time = 0;
  int last_led_tick = 0;
  int flash_frequency = 0;
  bool flash_state = false;
  uint32_t last_led_state = 0;

  // Wait for MQTT connection
  while (uxSemaphoreGetCount(connection_state_semaphore) == 0)
  {
    vTaskDelay(100);
  }

  // Subscribe to the relevant topics
  assert(xQueueSend(sub_queue, &topic_GRANT, portMAX_DELAY) == pdPASS);
  assert(xQueueSend(sub_queue, &topic_DENY, portMAX_DELAY) == pdPASS);
  assert(xQueueSend(sub_queue, &topic_ISUC, portMAX_DELAY) == pdPASS);
  assert(xQueueSend(sub_queue, &topic_IFAL, portMAX_DELAY) == pdPASS);

  // Set BOOT
  handleConnect("BOOT");

  bool connected = true;
  for (;;)
  {
    // Did we disconnect?
    if (uxSemaphoreGetCount(connection_state_semaphore) == 0)
    {
      if (connected == true)
      {
        printf("Tool: Connection dropped\n");
      }
      if (state == DEV_IDLE)
      {
        state = DEV_NO_CONN;
      }
      connected = false;
    }
    // Did we reconnect?
    else if (!connected)
    {
      printf("Tool: Reconnect\n");
      if (state == DEV_IDLE)
      {
        handleConnect("IDLE");
      }
      else
      {
        handleConnect("ACTIVE");
      }
      connected = true;
    }

    // Anything from MQTT?
    mqtt_message_t incoming;
    if (xQueueReceive(incoming_queue, &incoming, 0) == pdPASS)
    {
      printf("Tool: Incoming Message: Topic: %s Payload: %s\n", incoming.topic, incoming.payload);
      handleMqttMessage(incoming.topic, incoming.payload, strlen(incoming.payload));
    }

    // Anything from the reader?
    MFRC522::Uid card_id;
    if (xQueueReceive(uid_queue, &card_id, 0) == pdPASS)
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

        handleNewCard(card_id.uidByte, card_id.size);
    }
    
    // Any buttons?
    button_event_t button_event;
    if (xQueueReceive(button_queue, &button_event, 0) == pdPASS)
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

    // Timed out?
    if (xSemaphoreTake(timeout_semaphore, 0) == pdTRUE)
    {
      handleTimeout();
    }

    // Calculate LED flash state
    const int led_tick= xTaskGetTickCount();
    if (led_tick > last_led_tick + pdMS_TO_TICKS(flash_frequency))
    {
      last_led_tick = led_tick;
      flash_state = !flash_state;
    }
    // Wrapped around or reset
    else if (last_led_tick > led_tick)
    {
      last_led_tick = led_tick;
      flash_state = true;
    }

    // Update LEDs and relays based on state
    if (state == DEV_NO_CONN)
    {
      // Off
      ledOff(PIN_LED_TOOL);
      ledOff(PIN_LED_INDUCT);

      relayOff();

      // Flash state
      flash_frequency = STATE_FLASH_FREQ;
      ledSet(PIN_LED_STATE, flash_state);
    }
    else if (state == DEV_IDLE)
    {
      // All off
      ledOff(PIN_LED_TOOL);
      ledOff(PIN_LED_INDUCT);
      ledOff(PIN_LED_STATE);

      relayOff();
    }
    else if (state == DEV_AUTH_WAIT)
    {
      // Flash state
      flash_frequency = STATE_FLASH_FREQ;
      ledSet(PIN_LED_STATE, flash_state);
    }
    else if (state == DEV_ACTIVE)
    {
      // Tool LED and relay on
      ledOn(PIN_LED_TOOL);
      relayOn();

      // State LED off
      ledOff(PIN_LED_STATE);

      if (is_inductor)
      {
        // Induct LED solid if we're an inductor
        ledOn(PIN_LED_INDUCT);
      }
    }
    else if (state == DEV_INDUCT)
    {
      // State LED solid
      ledOn(PIN_LED_STATE);

      // Induct LED flashes
      flash_frequency = INDUCT_FLASH_FREQ;
      ledSet(PIN_LED_INDUCT, flash_state);
    }
    else if (state == DEV_INDUCT_WAIT)
    {
      // Induct LED solid
      ledOn(PIN_LED_INDUCT);

      // State LED flashes
      flash_frequency = STATE_FLASH_FREQ;
      ledSet(PIN_LED_STATE, flash_state);
    }
    else
    {
      // Induct and Status LEDs off
      ledOff(PIN_LED_INDUCT);
      ledOff(PIN_LED_STATE);
    }

    // Update LEDs if the state changed
    if (led_state.led_state != last_led_state)
    {
      assert(xQueueSend(led_queue, &led_state, portMAX_DELAY) == pdPASS);
    }

    // Show session time every second while active
    if (state == DEV_ACTIVE)
    {
      const int session_time = getSessionTimeInMilliseconds();
      // At least a second passed
      if (session_time >= last_session_time + 1000)
      {
        printf(
          "Tool: Session Time: %02d:%02d:%02d\n",
          getSessionHours(),
          getSessionMinutes(),
          getSessionSeconds()
        );
        last_session_time = session_time;
      }
      // Wrapped around (unlikely) or started a new session, reset
      else if (session_time < last_session_time)
      {
        last_session_time = session_time;
      }
    }
  }
}

static void formatUid(char* buffer, size_t buffer_size, uint8_t uid[10], size_t size)
{
  if (size == 4)
  {
    snprintf(
      buffer,
      buffer_size,
      "%02x%02x%02x%02x", 
      uid[0],
      uid[1],
      uid[2],
      uid[3]
    );
  }
  else if (size == 7)
  {
    snprintf(
      buffer,
      buffer_size,
      "%02x%02x%02x%02x%02x%02x%02x", 
      uid[0],
      uid[1],
      uid[2],
      uid[3],
      uid[4],
      uid[5],
      uid[6]
    );
  }
  else if (size == 10)
  {
    snprintf(
      buffer,
      buffer_size,
      "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x", 
      uid[0],
      uid[1],
      uid[2],
      uid[3],
      uid[4],
      uid[5],
      uid[6],
      uid[7],
      uid[8],
      uid[9]
    );
  }
  else
  {
    snprintf(buffer, buffer_size, "ERR:%d", size);
  }
}

static bool cardSizeIsValid(size_t size)
{
  switch (size)
  {
    case 4:
    case 7:
    case 10:
    {
      return true;
    }
    default:
    {
      return false;
    }
  }
}

void Tool::handleNewCard(uint8_t uid[10], size_t size)
{
  mqtt_message_t message;
  memset(&message, 0, sizeof(message));

  // Ignore cards of invalid size
  if (!cardSizeIsValid(size))
  {
    printf("Tool: Unexpected card size %d\n", size);
    return;
  }
  
  if (state == DEV_ACTIVE)
  {
    // Card still present, reset timeout
    if (size == current_size && memcmp(uid, current_uid, sizeof(uint8_t) * size) == 0)
    {
      restartTimeout();
    }
    else
    {
      printf("Tool: Card changed!\n");
    }
  }
  else if (state == DEV_IDLE)
  {
    state = DEV_AUTH_WAIT;

    // Record UID
    memcpy(current_uid, uid, sizeof(uint8_t) * size);
    current_size = size;

    // Send AUTH
    message.topic = topic_AUTH;
    formatUid(message.payload, sizeof(message.payload), uid, size);
    assert(xQueueSend(pub_queue, &message, portMAX_DELAY) == pdPASS);

    // Start timeout
    setTimeout(AUTH_TIMEOUT * 1000);
  }
  else if (state == DEV_INDUCT)
  {
    if (size == current_size && memcmp(uid, current_uid, sizeof(uint8_t) * size) != 0)
    {
      state = DEV_INDUCT_WAIT;

      // Send INDUCT
      message.topic = topic_INDUCT;
      char current_uid_buffer[21];
      char new_uid_buffer[21];
      formatUid(current_uid_buffer, sizeof(current_uid_buffer), current_uid, current_size);
      formatUid(new_uid_buffer, sizeof(new_uid_buffer), uid, size);
      snprintf(
        message.payload,
        sizeof(message.payload),
        "%s:%s",
        current_uid_buffer,
        new_uid_buffer
      );
      assert(xQueueSend(pub_queue, &message, portMAX_DELAY) == pdPASS);

      // Start timeout
      setTimeout(INDUCT_TIMEOUT * 1000);
    }
    else
    {
      // Same card, did we mean to press induct yet?
      printf("Tool: Induct pressed but this is the same card...ignoring\n");
    }
  }
  else
  {
    printf("Tool: Unexpected card in state %d\n", state);
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

    // Work out active time
    uint64_t session_time = getSessionTimeInMilliseconds() / 1000;

    // Send COMPLETE
    mqtt_message_t message;
    memset(&message, 0, sizeof(message));

    message.topic = topic_COMPLETE;
    snprintf(message.payload, sizeof(message.payload), "%llu", session_time);
  
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

void Tool::handleConnect(const char* payload)
{
  if (state == DEV_NO_CONN)
  {
    state = DEV_IDLE;

    clearUid();

  }
  else {
    printf("Tool: Connect from unexpected state %d\n", state);
  }

  // Send RESET
  mqtt_message_t message;
  memset(&message, 0, sizeof(message));

  message.topic = topic_RESET;
  snprintf(message.payload, sizeof(message.payload), "%s", payload);

  assert(xQueueSend(pub_queue, &message, portMAX_DELAY) == pdPASS);
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

      // Set a new timeout to make sure the card is present
      setTimeout(SESSION_TIMEOUT * 1000);

      // Record session start time
      session_start = xTaskGetTickCount();
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

      clearTimeout();
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

      clearTimeout();
      clearUid();
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

      clearTimeout();

      printf("Tool: Induct Failed\n%s\n", payload);
    }
    else
    {
      printf("Tool: IFAL from unexpected state %d\n", state);
    }
  }
}

void Tool::ledOn(int index)
{
  led_state.led_state |= (1 << index);
}

void Tool::ledOff(int index)
{
  led_state.led_state &= ~(1 << index);
}

void Tool::ledSet(int index, bool state)
{
  if (state)
  {
    ledOn(index);
  }
  else
  {
    ledOff(index);
  }
}

void Tool::relayOn()
{
  // Relay on
  bool status = true;
  assert(xQueueSend(relay_queue, &status, portMAX_DELAY) == pdPASS);
}

void Tool::relayOff()
{
  // Relay off
  bool status = false;
  assert(xQueueSend(relay_queue, &status, portMAX_DELAY) == pdPASS);
}

void Tool::setTimeout(int timeout)
{
  // Stop timer, update timeout, and restart
  clearTimeout();
  xTimerChangePeriod(timeout_timer, pdMS_TO_TICKS(timeout), portMAX_DELAY);
}

void Tool::clearTimeout()
{
  // Stop the timer if running
  if (xTimerIsTimerActive(timeout_timer) != pdFALSE)
  {
    xTimerStop(timeout_timer, portMAX_DELAY);
  }
}

void Tool::restartTimeout()
{
  xTimerReset(timeout_timer, portMAX_DELAY);
}

void Tool::handleTimeout()
{
  if (state == DEV_AUTH_WAIT)
  {
    printf("Tool: AUTH timeout\n");
    state = DEV_IDLE;
  }
  else if (state == DEV_INDUCT_WAIT)
  {
    printf("Tool: INDUCT timeout\n");
    state = DEV_IDLE;
  }
  else if (state == DEV_ACTIVE)
  {
    printf("Tool: Card timeout\n");
    state = DEV_IDLE;
  }
  else
  {
    printf("Tool: Timeout from unexpected state %d\n", state);
  }
}

void Tool::clearUid()
{
  // Clear current UID
  memset(current_uid, 0, sizeof(uint8_t) * 10);
  current_size = 0;

  // Not an inductor
  is_inductor = false;
}

uint64_t Tool::getSessionTimeInMilliseconds()
{
  uint64_t session_time = 0;
  TickType_t session_end = xTaskGetTickCount();
  if (session_end >= session_start) // No wrap
  {
    session_time = session_end - session_start;
  }
  else
  {
    session_time = session_end + (((uint64_t)-1) - session_start);
  }
  return pdTICKS_TO_MS(session_time);
}

int Tool::getSessionHours()
{
  return (getSessionTimeInMilliseconds() / 3600000);
}

int Tool::getSessionMinutes()
{
  return (getSessionTimeInMilliseconds() / 60000) % 60;
}

int Tool::getSessionSeconds()
{
  return (getSessionTimeInMilliseconds() / 1000) % 60;
}

void vToolDispatcher(void* in_params)
{
  Tool* tool = reinterpret_cast<Tool*>(in_params);
  tool->runTask();
}

