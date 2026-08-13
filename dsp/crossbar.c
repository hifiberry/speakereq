/*
 * crossbar.c - Audio Channel Crossbar Mixer Implementation
 * 
 * Mix n input channels to m output channels using a gain matrix.
 */

#include "crossbar.h"
#include <string.h>

int crossbar_init(void *crossbar, int num_inputs, int num_outputs) {
    if (!crossbar || num_inputs < 1 || num_outputs < 1) {
        return -1;
    }
    
    // Set dimensions
    int *dims = (int *)crossbar;
    dims[0] = num_inputs;
    dims[1] = num_outputs;
    
    // Get pointer to gain matrix (starts after two ints)
    LADSPA_Data *gain = (LADSPA_Data *)((char *)crossbar + 2 * sizeof(int));
    
    // Initialize all gains to 0 (muted)
    int total_gains = num_outputs * num_inputs;
    memset(gain, 0, sizeof(LADSPA_Data) * total_gains);
    
    return 0;
}

int crossbar_set_gain(void *crossbar, int output, int input, LADSPA_Data gain_value) {
    if (!crossbar) {
        return -1;
    }
    
    int *dims = (int *)crossbar;
    int num_inputs = dims[0];
    int num_outputs = dims[1];
    
    if (output < 0 || output >= num_outputs || input < 0 || input >= num_inputs) {
        return -1;
    }
    
    // Get pointer to gain matrix
    LADSPA_Data *gain = (LADSPA_Data *)((char *)crossbar + 2 * sizeof(int));
    
    // Set gain: matrix is [output][input], flattened row-major
    gain[output * num_inputs + input] = gain_value;
    
    return 0;
}

LADSPA_Data crossbar_get_gain(void *crossbar, int output, int input) {
    if (!crossbar) {
        return 0.0f;
    }
    
    int *dims = (int *)crossbar;
    int num_inputs = dims[0];
    int num_outputs = dims[1];
    
    if (output < 0 || output >= num_outputs || input < 0 || input >= num_inputs) {
        return 0.0f;
    }
    
    // Get pointer to gain matrix
    LADSPA_Data *gain = (LADSPA_Data *)((char *)crossbar + 2 * sizeof(int));
    
    return gain[output * num_inputs + input];
}

void crossbar_process(void *crossbar,
                      const LADSPA_Data **inputs,
                      LADSPA_Data **outputs,
                      unsigned long num_samples) {
    if (!crossbar || !inputs || !outputs) {
        return;
    }
    
    int *dims = (int *)crossbar;
    int num_inputs = dims[0];
    int num_outputs = dims[1];
    
    // Get pointer to gain matrix
    LADSPA_Data *gain = (LADSPA_Data *)((char *)crossbar + 2 * sizeof(int));
    
    // Process each output channel
    for (int out = 0; out < num_outputs; out++) {
        // Clear output buffer
        memset(outputs[out], 0, sizeof(LADSPA_Data) * num_samples);
        
        // Mix all inputs to this output
        for (int in = 0; in < num_inputs; in++) {
            LADSPA_Data g = gain[out * num_inputs + in];
            
            // Skip if gain is zero (optimization)
            if (g == 0.0f) {
                continue;
            }
            
            // Accumulate: output += input * gain
            for (unsigned long i = 0; i < num_samples; i++) {
                outputs[out][i] += inputs[in][i] * g;
            }
        }
    }
}

int crossbar_set_identity(void *crossbar) {
    if (!crossbar) {
        return -1;
    }
    
    int *dims = (int *)crossbar;
    int num_inputs = dims[0];
    int num_outputs = dims[1];
    
    if (num_inputs != num_outputs) {
        return -1;  // Identity only valid for square matrices
    }
    
    // Get pointer to gain matrix
    LADSPA_Data *gain = (LADSPA_Data *)((char *)crossbar + 2 * sizeof(int));
    
    // Set diagonal to 1.0, all others to 0.0
    for (int out = 0; out < num_outputs; out++) {
        for (int in = 0; in < num_inputs; in++) {
            gain[out * num_inputs + in] = (out == in) ? 1.0f : 0.0f;
        }
    }
    
    return 0;
}
