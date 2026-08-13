/*
 * filterbank.h - Biquad Filter Bank
 * 
 * Array of biquad filters for parametric EQ implementation.
 * Processes audio through multiple cascaded biquad stages.
 * 
 * Usage: Define filterbank with specific size:
 *   FILTERBANK(12) my_filterbank;  // Creates filterbank with 12 bands
 */

#ifndef FILTERBANK_H
#define FILTERBANK_H

#include "biquad.h"

// Macro to define a filterbank with specific number of bands
#define FILTERBANK(n) struct { \
    int num_bands; \
    BiquadCoeffs coeffs[n]; \
    BiquadState state[n]; \
    int enabled[n]; \
}

// Generic filterbank pointer type for API functions
typedef struct {
    int num_bands;
    BiquadCoeffs *coeffs;
    BiquadState *state;
    int *enabled;
} FilterBankHeader;

/**
 * Initialize a filter bank
 * 
 * @param bank Pointer to FilterBank structure (use &your_filterbank)
 * @param num_bands Number of filter bands in the structure
 * @return 0 on success, -1 on error
 */
int filterbank_init(void *bank, int num_bands);

/**
 * Reset all filter states to zero (clear history)
 * 
 * @param bank Pointer to FilterBank structure
 */
void filterbank_reset(void *bank);

/**
 * Set coefficients for a specific filter band
 * 
 * @param bank Pointer to FilterBank structure
 * @param band_index Band index (0 to num_bands-1)
 * @param coeffs Pointer to biquad coefficients to copy
 * @return 0 on success, -1 if band_index is out of range
 */
int filterbank_set_band(void *bank, int band_index, const BiquadCoeffs *coeffs);

/**
 * Enable or disable a specific filter band
 * 
 * @param bank Pointer to FilterBank structure
 * @param band_index Band index (0 to num_bands-1)
 * @param enabled 1 to enable the band, 0 to bypass it
 * @return 0 on success, -1 if band_index is out of range
 */
int filterbank_set_band_enabled(void *bank, int band_index, int enabled);

/**
 * Get the enabled state of a specific filter band
 * 
 * @param bank Pointer to FilterBank structure
 * @param band_index Band index (0 to num_bands-1)
 * @return 1 if enabled, 0 if disabled/bypassed, -1 on error
 */
int filterbank_get_band_enabled(void *bank, int band_index);

/**
 * Process a single sample through all filter bands in series
 * 
 * @param bank Pointer to FilterBank structure
 * @param sample Input sample
 * @return Filtered output sample
 */
float filterbank_process_sample(void *bank, float sample);

/**
 * Process a buffer of samples through all filter bands
 * 
 * @param bank Pointer to FilterBank structure
 * @param input Input buffer
 * @param output Output buffer (can be same as input for in-place processing)
 * @param num_samples Number of samples to process
 */
void filterbank_process_buffer(void *bank, const float *input, float *output, unsigned long num_samples);

#endif // FILTERBANK_H
