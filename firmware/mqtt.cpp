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

#include "mqtt.h"

static void wifi_connect(const char* ssid, const char* password, ip_addr_t ip_addr)
{
  printf("Connecting...\n");

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
		printf("Failed to join AP. Attempt %d\n", attempt);
		vTaskDelay(2000);
	}

  uint8_t mac[6];
  cyw43_wifi_get_mac(&cyw43_state, CYW43_ITF_STA, mac);
  printf("MAC Address: %x:%x:%x:%x:%x:%x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  uint8_t ip[4];
  memcpy(ip, netif_ip4_addr(&cyw43_state.netif[0]), 4);
  printf("IP Address: %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);

  printf("Connected.\n");

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

static void mqtt_connect(mqtt_client_t* client, mqtt_task_params_t* params)
{
  struct mqtt_connect_client_info_t ci;
  err_t err;

  memset(&ci, 0, sizeof(ci));

  ci.client_id = params->device_name;
  ci.keep_alive = 30;

  err = mqtt_client_connect(client, &params->server, params->port, mqtt_connection_cb, params, &ci);

  if (err != ERR_OK)
  {
    printf("mqtt_connect returned %d\n", err);
  }
}

static void mqtt_sub_request_cb(void *arg, err_t result)
{
  /* Just print the result code here for simplicity, 
     normal behaviour would be to take some action if subscribe fails like 
     notifying user, retry subscribe or disconnect from server */
  printf("Subscribe result: %d\n", result);
}

/* The idea is to demultiplex topic and create some reference to be used in data callbacks
   Example here uses a global variable, better would be to use a member in arg
   If RAM and CPU budget allows it, the easiest implementation might be to just take a copy of
   the topic string and use it in mqtt_incoming_data_cb
*/
static int inpub_id;
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len)
{
  printf("Incoming publish at topic %s with total length %u\n", topic, (unsigned int)tot_len);

  /* Decode topic string into a user defined reference */
  if(strcmp(topic, "subtopic") == 0) {
    inpub_id = 0;
  } else if(topic[0] == 'A') {
    /* All topics starting with 'A' might be handled at the same way */
    inpub_id = 1;
  } else {
    /* For all other topics */
    inpub_id = 2;
  }
}

static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
  printf("Incoming publish payload with length %d, flags %u\n", len, (unsigned int)flags);

  if(flags & MQTT_DATA_FLAG_LAST) {
    /* Last fragment of payload received (or whole part if payload fits receive buffer
       See MQTT_VAR_HEADER_BUFFER_LEN)  */

    /* Call function or do action depending on reference, in this case inpub_id */
    if(inpub_id == 0) {
      /* Don't trust the publisher, check zero termination */
      printf("mqtt_incoming_data_cb: ");
      for (int i = 0; i < len; ++i)
      {
        printf("%c", data[i]);
      }
      printf("\n");
    } else if(inpub_id == 1) {
      /* Call an 'A' function... */
    } else {
      printf("mqtt_incoming_data_cb: Ignoring payload...\n");
    }
  } else {
    /* Handle fragmented payload, store in buffer, write to file or whatever */
  }
}

static void mqtt_connection_cb(mqtt_client_t* client, void* arg, mqtt_connection_status_t status)
{
  err_t err;
  if (status == MQTT_CONNECT_ACCEPTED)
  {
    printf("mqtt_connection_cb: Successfully connected\n");
    
    /* Setup callback for incoming publish requests */
    mqtt_set_inpub_callback(client, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, arg);
    
    /* Subscribe to a topic named "subtopic" with QoS level 1, call mqtt_sub_request_cb with result */ 
    err = mqtt_subscribe(client, "subtopic", 1, mqtt_sub_request_cb, arg);

    if(err != ERR_OK) {
      printf("mqtt_subscribe return: %d\n", err);
    }
  }
  else
  {
    printf("mqtt_connection_cb: Disconnected, reason: %d\n", status);
    
    //mqtt_connect(client, reinterpret_cast<mqtt_task_params_t*>(arg));
  }
}

void example_publish(mqtt_client_t *client, void *arg)
{
  const char *pub_payload= "PubSubHubLubJub";
  err_t err;
  u8_t qos = 2; /* 0 1 or 2, see MQTT specification */
  u8_t retain = 0; /* No don't retain such crappy payload... */
  err = mqtt_publish(client, "pub_topic", pub_payload, strlen(pub_payload), qos, retain, mqtt_pub_request_cb, arg);
  if(err != ERR_OK) {
    printf("Publish err: %d\n", err);
  }
}

/* Called when publish is complete either with sucess or failure */
static void mqtt_pub_request_cb(void *arg, err_t result)
{
  if(result != ERR_OK) {
    printf("Publish result: %d\n", result);
  }
}

//5. Disconnecting
//
//Simply call mqtt_disconnect(client)

// Publish
char* topic_AUTH     = nullptr;
char* topic_COMPLETE = nullptr;
char* topic_INDUCT   = nullptr;
char* topic_RESET    = nullptr;

// Subscribe
char* topic_GRANT    = nullptr;
char* topic_DENY     = nullptr;
char* topic_ISUC     = nullptr;
char* topic_IFAL     = nullptr;

void vMqttTask(void* in_params)
{
  mqtt_task_params_t params = *reinterpret_cast<mqtt_task_params_t*>(in_params);

  if (cyw43_arch_init() != 0)
  {
    printf("Failed to init wifi!");
  }

  wifi_connect(params.ssid, params.password, params.ip);

  mqtt_client_t* mqtt_client = mqtt_client_new();
  mqtt_connect(mqtt_client, &params);

  for (;;)
  {
    // Tick the network stack
    cyw43_arch_poll();

    if (!wifi_is_connected())
    {
      printf("Link dropped, reconnecting...\n");
      wifi_connect(params.ssid, params.password, params.ip);
    }

    if (!mqtt_client_is_connected(mqtt_client))
    {
      printf("MQTT client disconnected. PANIK?\n");
      vTaskDelay(1000);
    }

    vTaskDelay(50);
  }
}
