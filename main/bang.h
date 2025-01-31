#pragma once

// Chopping block - switching to UART if I can figure it out

#include "driver/gpio.h"
#include "rom/ets_sys.h"

#define BANG_GPIO GPIO_NUM_1

void bang_setup(void)
{
    gpio_reset_pin(BANG_GPIO);
    // I can receive data with GPIO_MODE_INPUT, but it's mangled.
    // When I set to INPUT_OUTPUT, neither input nor output work when
    // connected to host device.
    gpio_set_direction(BANG_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_intr_type(BANG_GPIO, GPIO_INTR_NEGEDGE);
}

void bang_zero(void)
{
    // LLLH
    gpio_set_level(BANG_GPIO, 0);
    ets_delay_us(3);
    gpio_set_level(BANG_GPIO, 1);
    ets_delay_us(1);
}

void bang_one(void)
{
    // LHHH
    gpio_set_level(BANG_GPIO, 0);
    ets_delay_us(1);
    gpio_set_level(BANG_GPIO, 1);
    ets_delay_us(3);
}

void bang_bit(bool bit)
{
    if (bit) {
        bang_one();
    } else {
        bang_zero();
    }
}

void bang_stop(void)
{
    // LLHH
    gpio_set_level(BANG_GPIO, 0);
    ets_delay_us(2);
    gpio_set_level(BANG_GPIO, 1);
    ets_delay_us(2);
}

void bang_byte(uint8_t value)
{
    // MSB First
    for (int i = 7; i >= 0; i--) {
        bang_bit((value >> i) & 1);
    }
}

// Response to 0x00 or 0xFF
void bang_info()
{
    // 0x5000 - N64 Controller ID
    bang_byte(0x50);
    bang_byte(0x00);

    // 0x00   - Flags (Ignore for now)
    bang_byte(0x00);
}

// Response to 0x01
void bang_state()
{
    // TODO: Make me a struct
    // A B Z S dU dD dL dR
    bang_byte(0x00);
    // RST _ LT RT cU cD cL cR
    bang_byte(0x00);
    // Stick X pos as signed byte
    bang_byte(0x00);
    // Stick Y pos as signed byte
    bang_byte(0x00);
}

void bang_test_packet()
{
    // Send mode, don't interrupt ourselves
    gpio_intr_disable(BANG_GPIO);
    // Rising edge at start of transmission
    gpio_set_level(BANG_GPIO, 1);
    ets_delay_us(1);

    bang_info();
    bang_stop();

    // Set level low at end of transmission
    gpio_set_level(BANG_GPIO, 0);

    // Listen for response
    gpio_intr_enable(BANG_GPIO);
}