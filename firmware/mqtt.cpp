extern "C" {
#include <stdio.h>
#include "FreeRTOS.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "lwip/apps/mqtt.h"
#include "pico/stdlib.h"
#include "config.h"
#include "tool.h"
#include "task.h"
#include "queue.h"
}

#include <vector>

#include "mqtt.h"

static void wifi_connect(const char* ssid, const char* password, ip_addr_t ip_addr)
{
  printf("MQTT: Connecting...\n");

  // LED off while attempting to connect
  cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);

  cyw43_wifi_pm(&cyw43_state ,CYW43_PERFORMANCE_PM);

  cyw43_arch_enable_sta_mode();

  // Are we using DHCP or do we have a static IP?
  // Use 0.0.0.0 to indicate DHCP
  if (!ip_addr_isany(&ip_addr))
  {
		dhcp_stop(cyw43_state.netif);

		netif_set_ipaddr( cyw43_state.netif, &ip_addr);
    //netif_set_netmask(...)
    //netif_set_gw(...)
  }

  for (uint8_t attempt = 1; 1; ++attempt)
  {
		if (cyw43_arch_wifi_connect_timeout_ms(ssid, password, CYW43_AUTH_WPA2_AES_PSK, 6000) == 0)
    {
      break;
    }
		printf("MQTT: Failed to join AP. Attempt %d\n", attempt);
		vTaskDelay(2000);
	}

  uint8_t mac[6];
  cyw43_wifi_get_mac(&cyw43_state, CYW43_ITF_STA, mac);
  printf("MQTT: MAC Address: %x:%x:%x:%x:%x:%x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  uint8_t ip[4];
  memcpy(ip, netif_ip4_addr(&cyw43_state.netif[0]), 4);
  printf("MQTT: IP Address: %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);

  printf("MQTT: Connected.\n");

  // LED on once connected
  cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
}

bool wifi_is_connected()
{
	return cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA) >= 0;
}

static void mqtt_sub_request_cb(void *arg, err_t result);
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len);
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags);
static void mqtt_connection_cb(mqtt_client_t* client, void* arg, mqtt_connection_status_t status);
static void mqtt_pub_request_cb(void *arg, err_t result);

typedef struct
{
  std::vector<const char*> topic_pool;
  mqtt_message_t buffer;
  int offset;
  QueueHandle_t incoming_queue;
} mqtt_context_t;

static void mqtt_connect(mqtt_client_t* client, mqtt_task_params_t* params, mqtt_context_t* context)
{
  struct mqtt_connect_client_info_t ci;
  err_t err;

  memset(&ci, 0, sizeof(ci));

  ci.client_id = params->device_name;
  ci.keep_alive = 30;

  err = mqtt_client_connect(client, &params->server, params->port, mqtt_connection_cb, context, &ci);

  if (err != ERR_OK)
  {
    printf("MQTT: mqtt_connect returned %d\n", err);
  }
}

static void mqtt_sub_request_cb(void *arg, err_t result)
{
  /* Just print the result code here for simplicity, 
     normal behaviour would be to take some action if subscribe fails like 
     notifying user, retry subscribe or disconnect from server */
  printf("MQTT: Subscribe result: %d\n", result);
}

static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len)
{
  mqtt_context_t* context = reinterpret_cast<mqtt_context_t*>(arg);

  printf("MQTT: Incoming publish at topic %s with total length %u\n", topic, (unsigned int)tot_len);

  assert(context->buffer.topic == nullptr);  // Make sure we're not trampling another message for some reason
  
  // Replace the topic with one of the strings from our pool
  for (const char* pool_topic : context->topic_pool)
  {
    //printf("MQTT: Try Pool Topic: %s Context: %x\n", pool_topic, context);
    if (strcmp(pool_topic, topic) == 0)
    {
      // Found it, set the pointer in our context
      printf("MQTT: Found pool topic: %s for topic %s.\n", pool_topic, topic);
      context->buffer.topic = pool_topic;
      break;
    }
  }
  
  context->offset = 0;
}

static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
  mqtt_context_t* context = reinterpret_cast<mqtt_context_t*>(arg);

  printf("MQTT: Incoming publish payload with length %d, flags %u, %s\n", len, (unsigned int)flags, context->buffer.topic ? "ACCEPTED" : "REJECTED");

  // This message was rejected
  if (context->buffer.topic == nullptr)
  {
    return;
  }

  if(flags & MQTT_DATA_FLAG_LAST) {
    /* Last fragment of payload received (or whole part if payload fits receive buffer
       See MQTT_VAR_HEADER_BUFFER_LEN)  */

    for (int i = 0; i < len && context->offset < MESSAGE_LEN; ++i)
    {
      context->buffer.payload[context->offset++] = data[i];
    }

    // Force null termination - overlong messages will be truncated
    if (context->offset >= MESSAGE_LEN)
    {
      context->offset = MESSAGE_LEN - 1;
    }
    context->buffer.payload[context->offset] = '\0';

    // Enqueue the message
    assert(xQueueSend(context->incoming_queue, &context->buffer, portMAX_DELAY) == pdPASS);

    // Clear the buffer for the next message
    memset(&context->buffer, 0, sizeof(context->buffer));
  }
  else
  {
    /* Handle fragmented payload, store in buffer, write to file or whatever */
    for (int i = 0; i < len && context->offset < MESSAGE_LEN; ++i)
    {
      context->buffer.payload[context->offset++] = data[i];
    }
  }
}

static void mqtt_connection_cb(mqtt_client_t* client, void* arg, mqtt_connection_status_t status)
{
  err_t err;
  if (status == MQTT_CONNECT_ACCEPTED)
  {
    printf("MQTT: Client connected\n");
    
    /* Setup callback for incoming publish requests */
    mqtt_set_inpub_callback(client, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, arg);
  }
  else
  {
    printf("MQTT: disconnected, reason: %d\n", status);
  }
}

/* Called when publish is complete either with sucess or failure */
static void mqtt_pub_request_cb(void *arg, err_t result)
{
  if(result != ERR_OK) {
    printf("MQTT: Publish result: %d\n", result);
  }
}

void vMqttTask(void* in_params)
{
  mqtt_task_params_t params = *reinterpret_cast<mqtt_task_params_t*>(in_params);

  if (cyw43_arch_init() != 0)
  {
    printf("MQTT: Failed to init wifi!");
  }

  wifi_connect(params.ssid, params.password, params.ip);

  mqtt_context_t context =
  {
    .buffer = {
      .topic = nullptr,
    },
    .offset = 0,
    .incoming_queue = params.incoming_queue,
  };
  memset(context.buffer.payload, 0, sizeof(context.buffer.payload));
  context.topic_pool.reserve(4);

  mqtt_client_t* mqtt_client = mqtt_client_new();
  mqtt_connect(mqtt_client, &params, &context);

  for (;;)
  {
    // Tick the network stack
    cyw43_arch_poll();

    if (!wifi_is_connected())
    {
      printf("MQTT: Link dropped, reconnecting...\n");
      wifi_connect(params.ssid, params.password, params.ip);
    }

    if (mqtt_client_is_connected(mqtt_client))
    {
      // Signal connected
      if (uxSemaphoreGetCount(params.connection_state_semaphore) == 0)
      {
        xSemaphoreGive(params.connection_state_semaphore);
      }

      // Check for requests to subscribe
      const char* topic = nullptr;
      while (xQueueReceive(params.sub_queue, &topic, 50) == pdPASS)
      {
        // Add the topic to the topic pool so we can reuse the string pointer
        context.topic_pool.push_back(topic);

        // Subscribe to the topic
        printf("MQTT: Subscribe. Topic: %s\n", topic);
        err_t err = mqtt_subscribe(mqtt_client, topic, 1, mqtt_sub_request_cb, &context);
        if (err != ERR_OK)
        {
          printf("MQTT: mqtt_subscribe returned: %d\n", err);
        }
      }

      // Check for requests to publish
      mqtt_message_t message;
      while ( xQueueReceive(params.pub_queue, &message, 50) == pdPASS)
      {
        err_t err;
        u8_t qos = 2;    /* Deliver exactly once */
        u8_t retain = 0; /* Do not retain */
        printf("MQTT: Publish topic: %s payload: %s\n", message.topic, message.payload);
        err = mqtt_publish(mqtt_client, message.topic, message.payload, strlen(message.payload), qos, retain, mqtt_pub_request_cb, &context);
        if(err != ERR_OK) {
          printf("MQTT: Publish error: %d\n", err);
        }
      }
    }
    else
    {
      // Not connected, wait a bit
      printf("MQTT: client is not connected.\n");

      // Signal connection dropped
      if (uxSemaphoreGetCount(params.connection_state_semaphore) == 1)
      {
        xSemaphoreTake(params.connection_state_semaphore, 0);
      }

      vTaskDelay(500);
    }
  }
}
