/*
 * crossbar.h - Audio Channel Crossbar Mixer
 * 
 * Mix n input channels to m output channels using a gain matrix.
 * Each output is a weighted sum of all inputs.
 */

#ifndef CROSSBAR_H
#define CROSSBAR_H

#include <ladspa.h>

/**
 * Macro to define a crossbar with specific input/output counts
 * Creates a struct with num_inputs, num_outputs, and gain matrix
 * 
 * Usage: CROSSBAR(inputs, outputs) name;
 * Example: CROSSBAR(2, 4) my_crossbar;
 */
#define CROSSBAR(n_in, n_out) struct { \
    int num_inputs; \
    int num_outputs; \
    LADSPA_Data gain[n_out][n_in]; \
}

/**
 * Generic crossbar header for API functions
 */
typedef struct {
    int num_inputs;
    int num_outputs;
    LADSPA_Data *gain;  // Flattened matrix: [output][input]
} CrossbarHeader;

/**
 * Initialize a crossbar mixer
 * Sets all gains to 0 (muted)
 * 
 * @param crossbar Pointer to crossbar structure (use &your_crossbar)
 * @param num_inputs Number of input channels
 * @param num_outputs Number of output channels
 * @return 0 on success, -1 on error
 */
int crossbar_init(void *crossbar, int num_inputs, int num_outputs);

/**
 * Set gain for a specific input-to-output path
 * 
 * @param crossbar Pointer to crossbar structure
 * @param output Output channel index (0-based)
 * @param input Input channel index (0-based)
 * @param gain Gain value (1.0 = unity, 0.0 = mute)
 * @return 0 on success, -1 on error
 */
int crossbar_set_gain(void *crossbar, int output, int input, LADSPA_Data gain);

/**
 * Get gain for a specific input-to-output path
 * 
 * @param crossbar Pointer to crossbar structure
 * @param output Output channel index (0-based)
 * @param input Input channel index (0-based)
 * @return Gain value, or 0.0 on error
 */
LADSPA_Data crossbar_get_gain(void *crossbar, int output, int input);

/**
 * Process audio through the crossbar mixer
 * Each output is the sum of all inputs multiplied by their respective gains
 * 
 * @param crossbar Pointer to crossbar structure
 * @param inputs Array of pointers to input buffers
 * @param outputs Array of pointers to output buffers
 * @param num_samples Number of samples to process
 */
void crossbar_process(void *crossbar, 
                      const LADSPA_Data **inputs,
                      LADSPA_Data **outputs,
                      unsigned long num_samples);

/**
 * Set crossbar to identity routing (input N -> output N, gain 1.0)
 * Only works when num_inputs == num_outputs
 * All other paths are set to 0.0
 * 
 * @param crossbar Pointer to crossbar structure
 * @return 0 on success, -1 on error
 */
int crossbar_set_identity(void *crossbar);

#endif // CROSSBAR_H
