/*
 * Copyright (c) 2021 Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "mic-analog.h"

#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/irq.h"

#include <string.h>
#include <stdlib.h>

#define ANALOG_RAW_BUFFER_COUNT 2

static struct {
    int       dma_channel;
    uint16_t* raw_buffer[ANALOG_RAW_BUFFER_COUNT];
    uint32_t  buffer_size;
    int16_t   bias;
    uint32_t  dma_irq;

    volatile int raw_buffer_write_index;
    volatile int raw_buffer_read_index;

    analog_microphone_config config;
    analog_samples_ready_handler_t samples_ready_handler;
} analog_mic;

static void analog_dma_handler();

int analog_microphone_init(const struct analog_microphone_config* config) {
    memset(&analog_mic, 0x00, sizeof(analog_mic));
    memcpy(&analog_mic.config, config, sizeof(analog_mic.config));

    if (config->gpio < 26 || config->gpio > 29) {
        return -1;
    }

    size_t raw_buffer_size = config->sample_buffer_size * sizeof(analog_mic.raw_buffer[0][0]);

    analog_mic.buffer_size = config->sample_buffer_size;
    analog_mic.bias = ((int16_t)((config->bias_voltage * 4095) / 3.3));

    for (int i = 0; i < ANALOG_RAW_BUFFER_COUNT; i++) {
        analog_mic.raw_buffer[i] = (uint16_t* )malloc(raw_buffer_size);
        if (analog_mic.raw_buffer[i] == NULL) {
            analog_microphone_deinit();

            return -1;
        }
    }

    analog_mic.dma_channel = dma_claim_unused_channel(true);
    if (analog_mic.dma_channel < 0) {
        analog_microphone_deinit();

        return -1;
    }

    float clk_div = (clock_get_hz(clk_adc) / (1.0 * config->sample_rate)) - 1;

    dma_channel_config dma_channel_cfg = dma_channel_get_default_config(analog_mic.dma_channel);

    channel_config_set_transfer_data_size(&dma_channel_cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&dma_channel_cfg, false);
    channel_config_set_write_increment(&dma_channel_cfg, true);
    channel_config_set_dreq(&dma_channel_cfg, DREQ_ADC);

    analog_mic.dma_irq = DMA_IRQ_0;

    dma_channel_configure(
        analog_mic.dma_channel,
        &dma_channel_cfg,
        analog_mic.raw_buffer[0],
        &adc_hw->fifo,
        analog_mic.buffer_size,
        false
    );

    adc_gpio_init(config->gpio);

    adc_init();
    adc_select_input(config->gpio - 26);
    adc_fifo_setup(
        true,    // Write each completed conversion to the sample FIFO
        true,    // Enable DMA data request (DREQ)
        1,       // DREQ (and IRQ) asserted when at least 1 sample present
        false,   // We won't see the ERR bit because of 8 bit reads; disable.
        false    // Don't shift each sample to 8 bits when pushing to FIFO
    );

    adc_set_clkdiv(clk_div);

    return 0;
}

void analog_microphone_deinit() {
    for (int i = 0; i < ANALOG_RAW_BUFFER_COUNT; i++) {
        if (analog_mic.raw_buffer[i]) {
            free(analog_mic.raw_buffer[i]);

            analog_mic.raw_buffer[i] = NULL;
        }
    }

    if (analog_mic.dma_channel > -1) {
        dma_channel_unclaim(analog_mic.dma_channel);

        analog_mic.dma_channel = -1;
    }
}

int analog_microphone_start() {
    irq_set_enabled(analog_mic.dma_irq, true);
    irq_set_exclusive_handler(analog_mic.dma_irq, analog_dma_handler);

    if (analog_mic.dma_irq == DMA_IRQ_0) {
        dma_channel_set_irq0_enabled(analog_mic.dma_channel, true);
    } else if (analog_mic.dma_irq == DMA_IRQ_1) {
        dma_channel_set_irq1_enabled(analog_mic.dma_channel, true);
    } else {
        return -1;
    }

    analog_mic.raw_buffer_write_index = 0;
    analog_mic.raw_buffer_read_index = 0;

    dma_channel_transfer_to_buffer_now(
        analog_mic.dma_channel,
        analog_mic.raw_buffer[0],
        analog_mic.buffer_size
    );

    adc_run(true); // start running the adc
                   //
    return 0;
}

void analog_microphone_stop() {
    adc_run(false); // stop running the adc

    dma_channel_abort(analog_mic.dma_channel);

    if (analog_mic.dma_irq == DMA_IRQ_0) {
        dma_channel_set_irq0_enabled(analog_mic.dma_channel, false);
    } else if (analog_mic.dma_irq == DMA_IRQ_1) {
        dma_channel_set_irq1_enabled(analog_mic.dma_channel, false);
    }

    irq_set_enabled(analog_mic.dma_irq, false);
}

static void analog_dma_handler() {
    // clear IRQ
    if (analog_mic.dma_irq == DMA_IRQ_0) {
        dma_hw->ints0 = (1u << analog_mic.dma_channel);
    } else if (analog_mic.dma_irq == DMA_IRQ_1) {
        dma_hw->ints1 = (1u << analog_mic.dma_channel);
    }

    // get the current buffer index
    analog_mic.raw_buffer_read_index = analog_mic.raw_buffer_write_index;

    // get the next capture index to send the dma to start
    analog_mic.raw_buffer_write_index = (analog_mic.raw_buffer_write_index + 1) % ANALOG_RAW_BUFFER_COUNT;

    // give the channel a new buffer to write to and re-trigger it
    dma_channel_transfer_to_buffer_now(
        analog_mic.dma_channel,
        analog_mic.raw_buffer[analog_mic.raw_buffer_write_index],
        analog_mic.buffer_size
    );

    if (analog_mic.samples_ready_handler) {
        analog_mic.samples_ready_handler();
    }
}

void analog_microphone_set_samples_ready_handler(analog_samples_ready_handler_t handler) {
    analog_mic.samples_ready_handler = handler;
}

int analog_microphone_read(int16_t* buffer, size_t samples) {
    if (samples > analog_mic.config.sample_buffer_size) {
        samples = analog_mic.config.sample_buffer_size;
    }

    if (analog_mic.raw_buffer_write_index == analog_mic.raw_buffer_read_index) {
        return 0;
    }

    uint16_t* in = analog_mic.raw_buffer[analog_mic.raw_buffer_read_index];
    int16_t* out = buffer;
    int16_t bias = analog_mic.bias;

    analog_mic.raw_buffer_read_index++;

    for (int i = 0; i < samples; i++) {
        *out++ = *in++ - bias;
    }

    return samples;
}
