#pragma once

#include "common.h"

int n64_uart_init(void);

void n64_uart_set_state(const n64_controller_state_t* state);
