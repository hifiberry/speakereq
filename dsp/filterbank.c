/*
 * filterbank.c - Biquad Filter Bank Implementation
 * 
 * Cascaded biquad filters for parametric EQ.
 */

#include "filterbank.h"
#include <string.h>

int filterbank_init(void *bank, int num_bands) {
    if (!bank || num_bands < 1) {
        return -1;
    }
    
    // Cast to access num_bands field
    int *num_bands_ptr = (int *)bank;
    *num_bands_ptr = num_bands;
    
    // The coeffs array starts right after num_bands
    BiquadCoeffs *coeffs = (BiquadCoeffs *)((char *)bank + sizeof(int));
    
    // Initialize all coefficients to passthrough (no filtering)
    for (int i = 0; i < num_bands; i++) {
        // Passthrough: y[n] = x[n]
        coeffs[i].b0 = 1.0f;
        coeffs[i].b1 = 0.0f;
        coeffs[i].b2 = 0.0f;
        coeffs[i].a1 = 0.0f;
        coeffs[i].a2 = 0.0f;
    }
    
    // Get enabled array location (after state array)
    BiquadState *state = (BiquadState *)(coeffs + num_bands);
    int *enabled = (int *)(state + num_bands);
    
    // Initialize all bands as enabled by default
    for (int i = 0; i < num_bands; i++) {
        enabled[i] = 1;
    }
    
    // Clear all filter states
    filterbank_reset(bank);
    
    return 0;
}

void filterbank_reset(void *bank) {
    if (!bank) {
        return;
    }
    
    // Get num_bands and calculate state array location
    int num_bands = *(int *)bank;
    BiquadCoeffs *coeffs = (BiquadCoeffs *)((char *)bank + sizeof(int));
    BiquadState *state = (BiquadState *)(coeffs + num_bands);
    
    // Zero out all filter state history
    memset(state, 0, sizeof(BiquadState) * num_bands);
}

int filterbank_set_band(void *bank, int band_index, const BiquadCoeffs *coeffs_in) {
    if (!bank || !coeffs_in) {
        return -1;
    }
    
    int num_bands = *(int *)bank;
    if (band_index < 0 || band_index >= num_bands) {
        return -1;
    }
    
    // Get coeffs array location
    BiquadCoeffs *coeffs = (BiquadCoeffs *)((char *)bank + sizeof(int));
    
    // Copy coefficients
    coeffs[band_index] = *coeffs_in;
    
    return 0;
}

int filterbank_set_band_enabled(void *bank, int band_index, int enabled_value) {
    if (!bank) {
        return -1;
    }
    
    int num_bands = *(int *)bank;
    if (band_index < 0 || band_index >= num_bands) {
        return -1;
    }
    
    // Get enabled array location
    BiquadCoeffs *coeffs = (BiquadCoeffs *)((char *)bank + sizeof(int));
    BiquadState *state = (BiquadState *)(coeffs + num_bands);
    int *enabled = (int *)(state + num_bands);
    
    // Set enabled state (normalize to 0 or 1)
    enabled[band_index] = enabled_value ? 1 : 0;
    
    return 0;
}

int filterbank_get_band_enabled(void *bank, int band_index) {
    if (!bank) {
        return -1;
    }
    
    int num_bands = *(int *)bank;
    if (band_index < 0 || band_index >= num_bands) {
        return -1;
    }
    
    // Get enabled array location
    BiquadCoeffs *coeffs = (BiquadCoeffs *)((char *)bank + sizeof(int));
    BiquadState *state = (BiquadState *)(coeffs + num_bands);
    int *enabled = (int *)(state + num_bands);
    
    return enabled[band_index];
}

float filterbank_process_sample(void *bank, float sample) {
    if (!bank) {
        return sample;
    }
    
    int num_bands = *(int *)bank;
    BiquadCoeffs *coeffs = (BiquadCoeffs *)((char *)bank + sizeof(int));
    BiquadState *state = (BiquadState *)(coeffs + num_bands);
    int *enabled = (int *)(state + num_bands);
    
    float output = sample;
    
    // Process through each filter band in series (only if enabled)
    for (int i = 0; i < num_bands; i++) {
        if (enabled[i]) {
            output = process_biquad(&coeffs[i], &state[i], output);
        }
    }
    
    return output;
}

void filterbank_process_buffer(void *bank, const float *input, float *output, unsigned long num_samples) {
    if (!bank || !input || !output) {
        return;
    }
    
    for (unsigned long i = 0; i < num_samples; i++) {
        output[i] = filterbank_process_sample(bank, input[i]);
    }
}
