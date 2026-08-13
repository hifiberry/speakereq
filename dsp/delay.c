/*
 * delay.c - Simple audio delay line implementation
 */

#include "delay.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

int delay_init(Delay *delay, float delay_ms, uint32_t sample_rate) {
    if (!delay) {
        return -1;
    }
    
    // Clamp delay to maximum
    if (delay_ms > MAX_DELAY_MS) {
        fprintf(stderr, "Warning: Delay %.2f ms exceeds maximum %d ms, clamping\n", 
                delay_ms, MAX_DELAY_MS);
        delay_ms = MAX_DELAY_MS;
    }
    
    if (delay_ms < 0) {
        delay_ms = 0;
    }
    
    // Calculate delay in samples
    delay->delay_samples = (uint32_t)roundf((delay_ms / 1000.0f) * sample_rate);
    
    // Buffer size needs to be at least delay_samples + 1 for proper operation
    // Add extra space for safety
    delay->buffer_size = delay->delay_samples + 1;
    
    // Allocate buffer
    delay->buffer = (float *)calloc(delay->buffer_size, sizeof(float));
    if (!delay->buffer) {
        fprintf(stderr, "Error: Failed to allocate delay buffer of %u samples\n", 
                delay->buffer_size);
        return -1;
    }
    
    delay->write_pos = 0;
    
    return 0;
}

void delay_process(Delay *delay, const float *input, float *output, uint32_t num_samples) {
    if (!delay || !delay->buffer) {
        // If delay not initialized, pass through
        if (input != output) {
            memcpy(output, input, num_samples * sizeof(float));
        }
        return;
    }
    
    // Handle zero delay case (direct passthrough)
    if (delay->delay_samples == 0) {
        if (input != output) {
            memcpy(output, input, num_samples * sizeof(float));
        }
        return;
    }
    
    for (uint32_t i = 0; i < num_samples; i++) {
        // Calculate read position (write_pos - delay_samples)
        uint32_t read_pos = (delay->write_pos + delay->buffer_size - delay->delay_samples) 
                            % delay->buffer_size;
        
        // Read delayed sample
        output[i] = delay->buffer[read_pos];
        
        // Write new sample
        delay->buffer[delay->write_pos] = input[i];
        
        // Advance write position
        delay->write_pos = (delay->write_pos + 1) % delay->buffer_size;
    }
}

void delay_reset(Delay *delay) {
    if (!delay || !delay->buffer) {
        return;
    }
    
    // Clear buffer
    memset(delay->buffer, 0, delay->buffer_size * sizeof(float));
    delay->write_pos = 0;
}

void delay_free(Delay *delay) {
    if (!delay) {
        return;
    }
    
    if (delay->buffer) {
        free(delay->buffer);
        delay->buffer = NULL;
    }
    
    delay->buffer_size = 0;
    delay->write_pos = 0;
    delay->delay_samples = 0;
}
