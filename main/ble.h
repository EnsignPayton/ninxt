#pragma once

#include "common.h"

int n64_ble_init(void);

void n64_ble_register_state_cb(n64_controller_state_cb_t cb);
