/*
 * delay.h - Simple audio delay line
 * 
 * Provides a configurable delay for audio signals with dynamic buffer allocation.
 */

#ifndef DELAY_H
#define DELAY_H

#include <stdint.h>

// Maximum delay in milliseconds (prevents extremely large buffer allocations)
#define MAX_DELAY_MS 100

typedef struct {
    float *buffer;        // Circular buffer for delayed samples
    uint32_t buffer_size; // Size of buffer in samples
    uint32_t write_pos;   // Current write position
    uint32_t delay_samples; // Actual delay in samples
} Delay;

/**
 * Initialize a delay line
 * 
 * @param delay Pointer to Delay structure
 * @param delay_ms Delay time in milliseconds (clamped to MAX_DELAY_MS)
 * @param sample_rate Sample rate in Hz
 * @return 0 on success, -1 on error
 */
int delay_init(Delay *delay, float delay_ms, uint32_t sample_rate);

/**
 * Process audio through the delay line
 * 
 * @param delay Pointer to Delay structure
 * @param input Input buffer
 * @param output Output buffer
 * @param num_samples Number of samples to process
 */
void delay_process(Delay *delay, const float *input, float *output, uint32_t num_samples);

/**
 * Reset the delay line (clear buffer)
 * 
 * @param delay Pointer to Delay structure
 */
void delay_reset(Delay *delay);

/**
 * Free delay line resources
 * 
 * @param delay Pointer to Delay structure
 */
void delay_free(Delay *delay);

#endif // DELAY_H
