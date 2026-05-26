#pragma once

extern "C" {
#include "lwip/ip_addr.h"
#include "lwip/ip4_addr.h"
#include "lwip/ip6_addr.h"
}

struct settings_t
{
  ip_addr_t ip_addr;
  ip_addr_t server_addr;
  char device_name[32];
  char term1;
  char base_topic[64];
  char term2;
  char ssid[32];
  char term3;
  char password[63];
  char term4;
};

void init_settings();
const settings_t* get_settings();

