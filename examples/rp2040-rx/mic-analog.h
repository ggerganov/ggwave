/*
 * Copyright (c) 2021 Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _PICO_ANALOG_MICROPHONE_H_
#define _PICO_ANALOG_MICROPHONE_H_

#include <stdint.h>
#include <stddef.h>

typedef void (*analog_samples_ready_handler_t)(void);

struct analog_microphone_config {
    uint32_t gpio;
    float    bias_voltage;
    uint32_t sample_rate;
    uint32_t sample_buffer_size;
};

int analog_microphone_init(const struct analog_microphone_config* config);
void analog_microphone_deinit();

int analog_microphone_start();
void analog_microphone_stop();

void analog_microphone_set_samples_ready_handler(analog_samples_ready_handler_t handler);

int analog_microphone_read(int16_t* buffer, size_t samples);

#endif
