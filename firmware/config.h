#pragma once

#define WIFI_SSID ""
#define WIFI_PASSWORD ""

#define IP_ADDRESS ""
#define MQTT_ADDRESS ""
#define MQTT_BASE_TOPIC ""
#define TOOL_NAME ""

#define PIN_MISO 4
#define PIN_MOSI 7
#define PIN_SCK 6

#define PIN_RFID_SS 5
#define PIN_RFID_IRQ 13
#define PIN_RFID_RESET 22

#define PIN_RELAY_SET 14
#define PIN_RELAY_RESET 15

#define PIN_CARD_DETECT 16 // Not used
#define PIN_LED_INDUCT 17
#define PIN_LED_TOOL 18
#define PIN_LED_STATE 19
#define PIN_BUTTON_INDUCT 20
#define PIN_BUTTON_SIGNOFF 21

typedef struct
{
	int pin_number;
	int pin_dir;
	int initial_value;
} pin_config_t;

const pin_config_t pin_configs[] = {
	{ PIN_MISO,           GPIO_IN,     0 },
	{ PIN_MOSI,           GPIO_OUT,    0 },
	{ PIN_SCK,            GPIO_OUT,    0 },
	{ PIN_RFID_SS,        GPIO_OUT,    0 },
	{ PIN_RFID_IRQ,       GPIO_IN,     0 },
	{ PIN_RFID_RESET,     GPIO_OUT,    0 },
	{ PIN_RELAY_SET,      GPIO_OUT,    0 },
	{ PIN_RELAY_RESET,    GPIO_OUT,    0 },
	{ PIN_CARD_DETECT,    GPIO_IN,     0 },
	{ PIN_LED_INDUCT,     GPIO_OUT,    0 },
	{ PIN_LED_TOOL,       GPIO_OUT,    0 },
	{ PIN_LED_STATE,      GPIO_OUT,    0 },
	{ PIN_BUTTON_INDUCT,  GPIO_IN,     0 },
	{ PIN_BUTTON_SIGNOFF, GPIO_IN,     0 },
};
const int num_pin_configs = sizeof(pin_configs) / sizeof(pin_configs[0]);

