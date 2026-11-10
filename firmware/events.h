#pragma once

typedef struct
{
  uint32_t led_state;
} led_event_t;

typedef struct
{
  uint32_t button_state;
  uint32_t change_mask;
} button_event_t;

