#pragma once

#include <inttypes.h>
#include "sdkconfig.h"

typedef struct {
    uint16_t buttons;
    int8_t x_axis;
    int8_t y_axis;
} n64_controller_state_t;

typedef void (*n64_controller_state_cb_t)(const n64_controller_state_t* state);
