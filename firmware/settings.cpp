extern "C" {
#include <stdio.h>
#include <cstring>
#include "FreeRTOS.h"
#include "hardware/flash.h"
#include "hardware/watchdog.h"
#include "pico/bootrom.h"
#include "pico/flash.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "config.h"
#include "task.h"
}

#include "settings.h"

extern uint32_t ADDR_RESERVED[];
#define NVM_BASE_ADDR ((uint32_t)ADDR_RESERVED - XIP_BASE)

static settings_t settings;

static void load_settings()
{
  memcpy(&settings, (uint8_t*)&ADDR_RESERVED, sizeof(settings_t));
  settings.term1 = '\0';
  settings.term2 = '\0';
  settings.term3 = '\0';
  settings.term4 = '\0';
}

static void __not_in_flash_func(save_settings())
{
	vTaskSuspendAll();
	taskENTER_CRITICAL();

  flash_range_erase(NVM_BASE_ADDR, FLASH_SECTOR_SIZE);
  flash_range_program(NVM_BASE_ADDR, reinterpret_cast<uint8_t*>(&settings), FLASH_PAGE_SIZE);

  printf("SETTINGS SAVED\n");

	watchdog_reboot(0, 0, 0);

	taskEXIT_CRITICAL();	// should never get here!!!!
	xTaskResumeAll();
}

static void show_help()
{
  printf("Commands:\n");
  printf("    show -     show current settings\n");
  printf("    ip -       set device IP address\n");
  printf("    server -   set server IP address\n");
  printf("    name -     set device name\n");
  printf("    base -     set base topic\n");
  printf("    ssid -     set wifi SSID\n");
  printf("    password - set wifi password\n");
  printf("    reboot -   reboot and reload settings\n");
  printf("\n");
}

static void print_ip_addr(ip_addr_t addr)
{
  if (IP_IS_V4(addr))
  {
    uint32_t addr_v4 = ip4_addr_get_u32(&addr);
    printf("%d.%d.%d.%d", (addr_v4) & 0xff, (addr_v4 >> 8) & 0xff, (addr_v4 >> 16) & 0xff, (addr_v4 >> 24) & 0xff);
  }
  else if (IP_IS_V6(addr))
  {
    printf("<IPV6>");
  }
  else
  {
    printf("<NOT SET>");
  }
}

static void show_settings()
{
  printf("Current Settings:\n");

  // Decode IP address
  printf("    ip address:     ");
  print_ip_addr(settings.ip_addr);
  printf("\n");

  // Decode server address
  printf("    server address: ");
  print_ip_addr(settings.server_addr);
  printf("\n");

  // Force null termination
  settings.term1 = '\0';
  settings.term2 = '\0';
  settings.term3 = '\0';
  settings.term4 = '\0';

  // Device name
  printf("    device name:    %s\n", settings.device_name);

  // Base topic
  printf("    base topic:     %s\n", settings.base_topic);

  // WiFi SSID
  printf("    ssid:           %s\n", settings.ssid);

  // WiFi Password
  printf("    password:       ");
  const int password_length = strlen(settings.password);
  for (int character = 0; character < password_length; ++character)
  {
    printf("*");
  }
  printf("\n");

  printf("\n");
}

static void set_ip_address(char* buffer)
{
  ipaddr_aton(buffer, &settings.ip_addr);
  printf("new ip addr: ");
  print_ip_addr(settings.ip_addr);
  printf("\n\n");
}

static void set_server_address(char* buffer)
{
  ipaddr_aton(buffer, &settings.server_addr);
  printf("new server addr: ");
  print_ip_addr(settings.server_addr);
  printf("\n\n");
}

static void set_name(char* buffer)
{
  strncpy(settings.device_name, buffer, sizeof(settings.device_name) / sizeof(settings.device_name[0]));
  settings.term1 = '\0';
  printf("new device name: %s\n\n", settings.device_name);
}

static void set_base_topic(char* buffer)
{
  strncpy(settings.base_topic, buffer, sizeof(settings.base_topic) / sizeof(settings.base_topic[0]));
  settings.term2 = '\0';
  printf("new base topic: %s\n\n", settings.base_topic);
}

static void set_ssid(char* buffer)
{
  strncpy(settings.ssid, buffer, sizeof(settings.ssid) / sizeof(settings.ssid[0]));
  settings.term3 = '\0';
  printf("new ssid: %s\n\n", settings.ssid);
}

static void set_password(char* buffer)
{
  strncpy(settings.password, buffer, sizeof(settings.password) / sizeof(settings.password[0]));
  settings.term4 = '\0';
  printf("new password: %s\n\n", settings.password);
}

static void reboot()
{
  printf("SAVE AND REBOOT\n\n");
  save_settings();
}

#define SHOW_COMMAND      "show"
#define IP_COMMAND        "ip "
#define SERVER_COMMAND    "server "
#define NAME_COMMAND      "name "
#define BASE_COMMAND      "base "
#define SSID_COMMAND      "ssid "
#define PASSWORD_COMMAND  "password "
#define REBOOT_COMMAND    "reboot"

static void process_command(char* command)
{
  if (!strcmp(command, SHOW_COMMAND))
  {
    show_settings();
  }
  else if (strstr(command, IP_COMMAND) == command)
  {
    set_ip_address(command + sizeof(IP_COMMAND) - 1);
  }
  else if (strstr(command, SERVER_COMMAND) == command)
  {
    set_server_address(command + sizeof(SERVER_COMMAND) - 1);
  }
  else if (strstr(command, NAME_COMMAND) == command)
  {
    set_name(command + sizeof(NAME_COMMAND) - 1);
  }
  else if (strstr(command, BASE_COMMAND) == command)
  {
    set_base_topic(command + sizeof(BASE_COMMAND) - 1);
  }
  else if (strstr(command, SSID_COMMAND) == command)
  {
    set_ssid(command + sizeof(SSID_COMMAND) - 1);
  }
  else if (strstr(command, PASSWORD_COMMAND) == command)
  {
    set_password(command + sizeof(PASSWORD_COMMAND) - 1);
  }
  else if (!strcmp(command, REBOOT_COMMAND))
  {
    reboot();
  }
  else
  {
    show_help();
  }
}

// Enough for the longest command and the longest value (63 character wifi password)
#define BUFFER_LEN 128

void vMenuTask(void* params)
{
  char buffer[BUFFER_LEN];
  size_t index = 0;
  buffer[index] = '\0';

  for (;;)
  {
    // About to overflow, throw away all input and start again
    if (index >= BUFFER_LEN - 1)
    {
      index = 0;
      buffer[index] = '\0';
    }

    // Any input available?
    int input = getchar_timeout_us(0);
    if (input != PICO_ERROR_TIMEOUT)
    {
      // If input is available, copy to the buffer then terminate the buffer
      char inchar = static_cast<char>(input);

      // Newline, process the command
      if (inchar == '\r')
      {
        process_command(buffer);
        index = 0;
        buffer[index] = '\0';
      }
      // Otherwise accumulate input
      else
      {
        buffer[index++] = inchar;
        buffer[index] = '\0';
      }
    }
    
    vTaskDelay(50);
  }
}

void init_settings()
{
  load_settings();

  xTaskCreate(vMenuTask, "Menu Task", 1024, nullptr, 3, nullptr);
}

const settings_t* get_settings()
{
  return &settings;
}


