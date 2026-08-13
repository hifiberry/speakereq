/*
 * speakereq2x2_ladspa.c - Speaker EQ LADSPA Plugin
 * 
 * 2-input, 2-output audio plugin for speaker equalization.
 * 
 * Signal flow: Input -> Input Gain -> Input Filterbank -> Crossbar -> Output Filterbank -> Delay -> Output Gain -> Master Volume -> Output
 * 
 * Port structure:
 * - 4 audio ports (2 inputs, 2 outputs)
 * - 200 input EQ control ports (2 channels × 20 bands × 5 params: type, freq, q, gain, enabled)
 * - 4 crossbar gain control ports
 * - 200 output EQ control ports (2 channels × 20 bands × 5 params: type, freq, q, gain, enabled)
 * - 2 delay control ports
 * - 4 gain control ports (input L/R, output L/R)
 * - 1 master volume control
 * - 1 save settings control
 * - 1 licensed output (always 1, retained for API compatibility)
 * Total: 418 ports
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <ladspa.h>
#include "dsp/biquad.h"
#include "dsp/filterbank.h"
#include "dsp/crossbar.h"
#include "dsp/delay.h"
#include "speakereq2x2_ladspa.h"
#include "utils/configfile.h"

#define NUM_EQ_BANDS 20

// Base port indices
#define PORT_ENABLE     0
#define PORT_INPUT_L    1
#define PORT_INPUT_R    2
#define PORT_OUTPUT_L   3
#define PORT_OUTPUT_R   4
#define PORT_CONTROL_BASE 5

// Macro to calculate port index for EQ parameters
// input: 0 or 1 (left/right), band: 0-19, param: 0=type, 1=freq, 2=q, 3=gain, 4=enabled
#define EQ_PORT(input, band, param) (PORT_CONTROL_BASE + (input) * NUM_EQ_BANDS * 5 + (band) * 5 + (param))

// Individual parameter offsets
#define PARAM_TYPE    0
#define PARAM_FREQ    1
#define PARAM_Q       2
#define PARAM_GAIN    3
#define PARAM_ENABLED 4

// Crossbar port indices (after all input EQ ports)
#define PORT_XBAR_BASE (PORT_CONTROL_BASE + 2 * NUM_EQ_BANDS * 5)
#define PORT_XBAR_L_TO_L (PORT_XBAR_BASE + 0)
#define PORT_XBAR_L_TO_R (PORT_XBAR_BASE + 1)
#define PORT_XBAR_R_TO_L (PORT_XBAR_BASE + 2)
#define PORT_XBAR_R_TO_R (PORT_XBAR_BASE + 3)

// Output EQ port indices (after crossbar)
#define PORT_OUTPUT_EQ_BASE (PORT_XBAR_BASE + 4)
#define OUTPUT_EQ_PORT(output, band, param) (PORT_OUTPUT_EQ_BASE + (output) * NUM_EQ_BANDS * 5 + (band) * 5 + (param))

// Delay port indices (after output EQ)
#define PORT_DELAY_BASE (PORT_OUTPUT_EQ_BASE + 2 * NUM_EQ_BANDS * 5)
#define PORT_DELAY_L (PORT_DELAY_BASE + 0)
#define PORT_DELAY_R (PORT_DELAY_BASE + 1)

// Gain port indices (after delay)
#define PORT_GAIN_BASE (PORT_DELAY_BASE + 2)
#define PORT_INPUT_GAIN_L (PORT_GAIN_BASE + 0)
#define PORT_INPUT_GAIN_R (PORT_GAIN_BASE + 1)
#define PORT_OUTPUT_GAIN_L (PORT_GAIN_BASE + 2)
#define PORT_OUTPUT_GAIN_R (PORT_GAIN_BASE + 3)
#define PORT_MASTER_VOLUME (PORT_GAIN_BASE + 4)
#define PORT_SAVE_SETTINGS (PORT_GAIN_BASE + 5)

#define PORT_LICENSED (PORT_GAIN_BASE + 6)

// Total number of ports: 1 enable + 4 audio + 200 input EQ (2ch*20bands*5params) + 4 crossbar + 200 output EQ (2ch*20bands*5params) + 2 delay + 4 gain + 1 master volume + 1 save settings + 1 licensed
#define TOTAL_PORTS (1 + 4 + 2 * NUM_EQ_BANDS * 5 + 4 + 2 * NUM_EQ_BANDS * 5 + 2 + 4 + 1 + 1 + 1)

typedef struct {
    // Control ports
    LADSPA_Data *enable;
    LADSPA_Data *licensed;
    
    LADSPA_Data *input_l;
    LADSPA_Data *input_r;
    LADSPA_Data *output_l;
    LADSPA_Data *output_r;
    
    // Input EQ parameters for all bands [channel][band][param]
    LADSPA_Data *input_eq_params[2][NUM_EQ_BANDS][5];
    
    // Crossbar gain controls
    LADSPA_Data *xbar_gain_l_to_l;
    LADSPA_Data *xbar_gain_l_to_r;
    LADSPA_Data *xbar_gain_r_to_l;
    LADSPA_Data *xbar_gain_r_to_r;
    
    // Output EQ parameters for all bands [channel][band][param]
    LADSPA_Data *output_eq_params[2][NUM_EQ_BANDS][5];
    
    // Delay controls (in milliseconds)
    LADSPA_Data *delay_ms_l;
    LADSPA_Data *delay_ms_r;
    
    // Current delay values (for detecting changes)
    float current_delay_l;
    float current_delay_r;
    
    // Gain controls (in dB)
    LADSPA_Data *input_gain_l_db;
    LADSPA_Data *input_gain_r_db;
    LADSPA_Data *output_gain_l_db;
    LADSPA_Data *output_gain_r_db;
    LADSPA_Data *master_volume;
    LADSPA_Data *save_settings;
    
    // Cached gain multipliers (linear)
    float input_gain_l_mult;
    float input_gain_r_mult;
    float output_gain_l_mult;
    float output_gain_r_mult;
    float master_volume_mult;
    
    // Configuration file support
    PluginConfig config;
    char *config_path;
    float last_save_settings_value;
    int config_applied;  // Flag to track if config has been applied on first activation
    
    float enable_internal; // Internal enable state
    
    unsigned long sample_rate;
    
    // Input filter banks for left and right channels (20 bands each)
    FILTERBANK(20) input_filterbank_l;
    FILTERBANK(20) input_filterbank_r;
    
    // Crossbar mixer (2 inputs, 2 outputs)
    CROSSBAR(2, 2) crossbar;
    
    // Output filter banks for left and right channels (20 bands each)
    FILTERBANK(20) output_filterbank_l;
    FILTERBANK(20) output_filterbank_r;
    
    // Output delays for left and right channels
    Delay delay_l;
    Delay delay_r;
} SpeakerEQ2x2;

static LADSPA_Handle instantiate_SpeakerEQ2x2(
    const LADSPA_Descriptor *descriptor,
    unsigned long sample_rate) {
    
    SpeakerEQ2x2 *plugin = (SpeakerEQ2x2 *)malloc(sizeof(SpeakerEQ2x2));
    if (plugin != NULL) {
        plugin->sample_rate = sample_rate;
        
        // Initialize all input_eq_params pointers to NULL
        memset(plugin->input_eq_params, 0, sizeof(plugin->input_eq_params));
        
        // Initialize all output_eq_params pointers to NULL
        memset(plugin->output_eq_params, 0, sizeof(plugin->output_eq_params));
        
        // Initialize crossbar gain pointers to NULL
        plugin->xbar_gain_l_to_l = NULL;
        plugin->xbar_gain_l_to_r = NULL;
        plugin->xbar_gain_r_to_l = NULL;
        plugin->xbar_gain_r_to_r = NULL;
        
        // Initialize gain pointers and multipliers
        plugin->input_gain_l_db = NULL;
        plugin->input_gain_r_db = NULL;
        plugin->output_gain_l_db = NULL;
        plugin->output_gain_r_db = NULL;
        plugin->input_gain_l_mult = 1.0f;   // 0 dB = 1x multiplier
        plugin->input_gain_r_mult = 1.0f;
        plugin->output_gain_l_mult = 1.0f;
        plugin->output_gain_r_mult = 1.0f;
        plugin->master_volume_mult = 1.0f;
        
        // Initialize input filterbanks (20 bands each)
        if (filterbank_init(&plugin->input_filterbank_l, 20) != 0) {
            fprintf(stderr, "SpeakerEQ2x2: Failed to initialize input left filterbank\n");
            free(plugin);
            return NULL;
        }
        if (filterbank_init(&plugin->input_filterbank_r, 20) != 0) {
            fprintf(stderr, "SpeakerEQ2x2: Failed to initialize input right filterbank\n");
            free(plugin);
            return NULL;
        }
        
        // Initialize crossbar (2x2 mixer)
        if (crossbar_init(&plugin->crossbar, 2, 2) != 0) {
            fprintf(stderr, "SpeakerEQ2x2: Failed to initialize crossbar\n");
            free(plugin);
            return NULL;
        }
        // Set default routing: identity (L->L, R->R)
        crossbar_set_identity(&plugin->crossbar);
        
        // Initialize output filterbanks (20 bands each)
        if (filterbank_init(&plugin->output_filterbank_l, 20) != 0) {
            fprintf(stderr, "SpeakerEQ2x2: Failed to initialize output left filterbank\n");
            free(plugin);
            return NULL;
        }
        if (filterbank_init(&plugin->output_filterbank_r, 20) != 0) {
            fprintf(stderr, "SpeakerEQ2x2: Failed to initialize output right filterbank\n");
            free(plugin);
            return NULL;
        }
        
        // Initialize output delays (0ms delay by default)
        plugin->current_delay_l = 0.0f;
        plugin->current_delay_r = 0.0f;
        if (delay_init(&plugin->delay_l, 0.0f, sample_rate) != 0) {
            fprintf(stderr, "SpeakerEQ2x2: Failed to initialize left delay\n");
            free(plugin);
            return NULL;
        }
        if (delay_init(&plugin->delay_r, 0.0f, sample_rate) != 0) {
            fprintf(stderr, "SpeakerEQ2x2: Failed to initialize right delay\n");
            delay_free(&plugin->delay_l);
            free(plugin);
            return NULL;
        }
        
        // Initialize configuration
        config_init(&plugin->config);
        plugin->config_path = config_build_path("speakereq2x2");
        plugin->last_save_settings_value = 0.0f;
        plugin->save_settings = NULL;
        plugin->config_applied = 0;  // Config not yet applied
        
        plugin->enable = NULL;
        plugin->licensed = NULL;
        plugin->enable_internal = 1.0f;  // Default enabled

        // Load configuration if it exists
        if (plugin->config_path) {
            config_load(plugin->config_path, &plugin->config);
        }
    }
    return (LADSPA_Handle)plugin;
}

static void connect_port_SpeakerEQ2x2(
    LADSPA_Handle instance,
    unsigned long port,
    LADSPA_Data *data) {
    
    SpeakerEQ2x2 *plugin = (SpeakerEQ2x2 *)instance;
    
    if (port == PORT_ENABLE) {
        plugin->enable = data;
    } else if (port == PORT_INPUT_L) {
        plugin->input_l = data;
    } else if (port == PORT_INPUT_R) {
        plugin->input_r = data;
    } else if (port == PORT_OUTPUT_L) {
        plugin->output_l = data;
    } else if (port == PORT_OUTPUT_R) {
        plugin->output_r = data;
    } else if (port == PORT_XBAR_L_TO_L) {
        plugin->xbar_gain_l_to_l = data;
    } else if (port == PORT_XBAR_L_TO_R) {
        plugin->xbar_gain_l_to_r = data;
    } else if (port == PORT_XBAR_R_TO_L) {
        plugin->xbar_gain_r_to_l = data;
    } else if (port == PORT_XBAR_R_TO_R) {
        plugin->xbar_gain_r_to_r = data;
    } else if (port == PORT_INPUT_GAIN_L) {
        plugin->input_gain_l_db = data;
    } else if (port == PORT_INPUT_GAIN_R) {
        plugin->input_gain_r_db = data;
    } else if (port == PORT_OUTPUT_GAIN_L) {
        plugin->output_gain_l_db = data;
    } else if (port == PORT_OUTPUT_GAIN_R) {
        plugin->output_gain_r_db = data;
    } else if (port == PORT_MASTER_VOLUME) {
        plugin->master_volume = data;
    } else if (port >= PORT_CONTROL_BASE && port < PORT_XBAR_BASE) {
        // Input EQ parameter ports
        int idx = port - PORT_CONTROL_BASE;
        int input = idx / (NUM_EQ_BANDS * 5);
        int remainder = idx % (NUM_EQ_BANDS * 5);
        int band = remainder / 5;
        int param = remainder % 5;
        
        if (input < 2 && band < NUM_EQ_BANDS && param < 5) {
            plugin->input_eq_params[input][band][param] = data;
        }
    } else if (port >= PORT_OUTPUT_EQ_BASE && port < PORT_DELAY_BASE) {
        // Output EQ parameter ports
        int idx = port - PORT_OUTPUT_EQ_BASE;
        int output = idx / (NUM_EQ_BANDS * 5);
        int remainder = idx % (NUM_EQ_BANDS * 5);
        int band = remainder / 5;
        int param = remainder % 5;
        
        if (output < 2 && band < NUM_EQ_BANDS && param < 5) {
            plugin->output_eq_params[output][band][param] = data;
        }
    } else if (port == PORT_DELAY_L) {
        plugin->delay_ms_l = data;
    } else if (port == PORT_DELAY_R) {
        plugin->delay_ms_r = data;
    } else if (port == PORT_SAVE_SETTINGS) {
        plugin->save_settings = data;
    } else if (port == PORT_LICENSED) {
        plugin->licensed = data;
    }
}

static void activate_SpeakerEQ2x2(LADSPA_Handle instance) {
    SpeakerEQ2x2 *plugin = (SpeakerEQ2x2 *)instance;
    
    // Reset all filter states
    filterbank_reset(&plugin->input_filterbank_l);
    filterbank_reset(&plugin->input_filterbank_r);
    filterbank_reset(&plugin->output_filterbank_l);
    filterbank_reset(&plugin->output_filterbank_r);
    
    // Reset delay lines
    delay_reset(&plugin->delay_l);
    delay_reset(&plugin->delay_r);
    
    // Apply loaded configuration values to control ports on first activation only
    // This provides default values from saved config file.
    // The host will then set its own parameter values which will override these.
    // On subsequent activations, we don't reapply config to preserve host's parameters.
    if (!plugin->config_applied && plugin->config.count > 0) {
        // Apply master volume
        if (plugin->master_volume) {
            float value = config_get_float(&plugin->config, "master_gain_db", *plugin->master_volume);
            *plugin->master_volume = value;
        }
        
        // Apply gain settings
        if (plugin->input_gain_l_db) {
            *plugin->input_gain_l_db = config_get_float(&plugin->config, "input_gain_0_db", *plugin->input_gain_l_db);
        }
        if (plugin->input_gain_r_db) {
            *plugin->input_gain_r_db = config_get_float(&plugin->config, "input_gain_1_db", *plugin->input_gain_r_db);
        }
        if (plugin->output_gain_l_db) {
            *plugin->output_gain_l_db = config_get_float(&plugin->config, "output_gain_0_db", *plugin->output_gain_l_db);
        }
        if (plugin->output_gain_r_db) {
            *plugin->output_gain_r_db = config_get_float(&plugin->config, "output_gain_1_db", *plugin->output_gain_r_db);
        }
        
        // Apply delay settings
        if (plugin->delay_ms_l) {
            *plugin->delay_ms_l = config_get_float(&plugin->config, "delay_0_ms", *plugin->delay_ms_l);
        }
        if (plugin->delay_ms_r) {
            *plugin->delay_ms_r = config_get_float(&plugin->config, "delay_1_ms", *plugin->delay_ms_r);
        }
        
        // Apply crossbar settings
        if (plugin->xbar_gain_l_to_l) {
            *plugin->xbar_gain_l_to_l = config_get_float(&plugin->config, "xbar_0_to_0", *plugin->xbar_gain_l_to_l);
        }
        if (plugin->xbar_gain_l_to_r) {
            *plugin->xbar_gain_l_to_r = config_get_float(&plugin->config, "xbar_0_to_1", *plugin->xbar_gain_l_to_r);
        }
        if (plugin->xbar_gain_r_to_l) {
            *plugin->xbar_gain_r_to_l = config_get_float(&plugin->config, "xbar_1_to_0", *plugin->xbar_gain_r_to_l);
        }
        if (plugin->xbar_gain_r_to_r) {
            *plugin->xbar_gain_r_to_r = config_get_float(&plugin->config, "xbar_1_to_1", *plugin->xbar_gain_r_to_r);
        }
        
        // Apply input EQ parameters
        for (int ch = 0; ch < 2; ch++) {
            for (int band = 0; band < NUM_EQ_BANDS; band++) {
                char key[64];
                if (plugin->input_eq_params[ch][band][PARAM_TYPE]) {
                    snprintf(key, sizeof(key), "input_%d_eq_%d_type", ch, band + 1);
                    *plugin->input_eq_params[ch][band][PARAM_TYPE] = config_get_float(&plugin->config, key, *plugin->input_eq_params[ch][band][PARAM_TYPE]);
                }
                if (plugin->input_eq_params[ch][band][PARAM_FREQ]) {
                    snprintf(key, sizeof(key), "input_%d_eq_%d_f", ch, band + 1);
                    *plugin->input_eq_params[ch][band][PARAM_FREQ] = config_get_float(&plugin->config, key, *plugin->input_eq_params[ch][band][PARAM_FREQ]);
                }
                if (plugin->input_eq_params[ch][band][PARAM_Q]) {
                    snprintf(key, sizeof(key), "input_%d_eq_%d_q", ch, band + 1);
                    *plugin->input_eq_params[ch][band][PARAM_Q] = config_get_float(&plugin->config, key, *plugin->input_eq_params[ch][band][PARAM_Q]);
                }
                if (plugin->input_eq_params[ch][band][PARAM_GAIN]) {
                    snprintf(key, sizeof(key), "input_%d_eq_%d_gain", ch, band + 1);
                    *plugin->input_eq_params[ch][band][PARAM_GAIN] = config_get_float(&plugin->config, key, *plugin->input_eq_params[ch][band][PARAM_GAIN]);
                }
            }
        }
        
        // Apply output EQ parameters
        for (int ch = 0; ch < 2; ch++) {
            for (int band = 0; band < NUM_EQ_BANDS; band++) {
                char key[64];
                if (plugin->output_eq_params[ch][band][PARAM_TYPE]) {
                    snprintf(key, sizeof(key), "output_%d_eq_%d_type", ch, band + 1);
                    *plugin->output_eq_params[ch][band][PARAM_TYPE] = config_get_float(&plugin->config, key, *plugin->output_eq_params[ch][band][PARAM_TYPE]);
                }
                if (plugin->output_eq_params[ch][band][PARAM_FREQ]) {
                    snprintf(key, sizeof(key), "output_%d_eq_%d_f", ch, band + 1);
                    *plugin->output_eq_params[ch][band][PARAM_FREQ] = config_get_float(&plugin->config, key, *plugin->output_eq_params[ch][band][PARAM_FREQ]);
                }
                if (plugin->output_eq_params[ch][band][PARAM_Q]) {
                    snprintf(key, sizeof(key), "output_%d_eq_%d_q", ch, band + 1);
                    *plugin->output_eq_params[ch][band][PARAM_Q] = config_get_float(&plugin->config, key, *plugin->output_eq_params[ch][band][PARAM_Q]);
                }
                if (plugin->output_eq_params[ch][band][PARAM_GAIN]) {
                    snprintf(key, sizeof(key), "output_%d_eq_%d_gain", ch, band + 1);
                    *plugin->output_eq_params[ch][band][PARAM_GAIN] = config_get_float(&plugin->config, key, *plugin->output_eq_params[ch][band][PARAM_GAIN]);
                }
            }
        }
        
        plugin->config_applied = 1;  // Mark config as applied
    }
}

static void run_SpeakerEQ2x2(
    LADSPA_Handle instance,
    unsigned long sample_count) {
    
    SpeakerEQ2x2 *plugin = (SpeakerEQ2x2 *)instance;
    LADSPA_Data *input_l = plugin->input_l;
    LADSPA_Data *input_r = plugin->input_r;
    LADSPA_Data *output_l = plugin->output_l;
    LADSPA_Data *output_r = plugin->output_r;
    
    // Licensed output port: retained for API compatibility, always 1
    if (plugin->licensed) {
        *plugin->licensed = (LADSPA_Data)1.0f;
    }

    // Handle enable control
    if (plugin->enable) {
        plugin->enable_internal = *plugin->enable;
    }

    // If disabled, bypass all processing
    if (plugin->enable_internal < 0.5f) {
        // Simple bypass: copy input to output
        for (unsigned long i = 0; i < sample_count; i++) {
            output_l[i] = input_l[i];
            output_r[i] = input_r[i];
        }
        return;
    }
    
    // Check for save settings trigger (transition from 0 to non-zero)
    if (plugin->save_settings && plugin->config_path) {
        float save_value = *plugin->save_settings;
        if (save_value > 0.5f && plugin->last_save_settings_value <= 0.5f) {
            // Triggered - save all current parameter values
            char value_str[32];
            
            // Save input EQ parameters
            for (int ch = 0; ch < 2; ch++) {
                for (int band = 0; band < NUM_EQ_BANDS; band++) {
                    if (plugin->input_eq_params[ch][band][PARAM_TYPE]) {
                        snprintf(value_str, sizeof(value_str), "input_%d_eq_%d_type", ch, band + 1);
                        char val[16];
                        snprintf(val, sizeof(val), "%d", (int)(*plugin->input_eq_params[ch][band][PARAM_TYPE] + 0.5f));
                        config_set(&plugin->config, value_str, val);
                    }
                    if (plugin->input_eq_params[ch][band][PARAM_FREQ]) {
                        snprintf(value_str, sizeof(value_str), "input_%d_eq_%d_f", ch, band + 1);
                        char val[16];
                        snprintf(val, sizeof(val), "%.1f", *plugin->input_eq_params[ch][band][PARAM_FREQ]);
                        config_set(&plugin->config, value_str, val);
                    }
                    if (plugin->input_eq_params[ch][band][PARAM_Q]) {
                        snprintf(value_str, sizeof(value_str), "input_%d_eq_%d_q", ch, band + 1);
                        char val[16];
                        snprintf(val, sizeof(val), "%.3f", *plugin->input_eq_params[ch][band][PARAM_Q]);
                        config_set(&plugin->config, value_str, val);
                    }
                    if (plugin->input_eq_params[ch][band][PARAM_GAIN]) {
                        snprintf(value_str, sizeof(value_str), "input_%d_eq_%d_gain", ch, band + 1);
                        char val[16];
                        snprintf(val, sizeof(val), "%.1f", *plugin->input_eq_params[ch][band][PARAM_GAIN]);
                        config_set(&plugin->config, value_str, val);
                    }
                }
            }
            
            // Save crossbar settings
            if (plugin->xbar_gain_l_to_l) {
                snprintf(value_str, sizeof(value_str), "%.3f", *plugin->xbar_gain_l_to_l);
                config_set(&plugin->config, "xbar_0_to_0", value_str);
            }
            if (plugin->xbar_gain_l_to_r) {
                snprintf(value_str, sizeof(value_str), "%.3f", *plugin->xbar_gain_l_to_r);
                config_set(&plugin->config, "xbar_0_to_1", value_str);
            }
            if (plugin->xbar_gain_r_to_l) {
                snprintf(value_str, sizeof(value_str), "%.3f", *plugin->xbar_gain_r_to_l);
                config_set(&plugin->config, "xbar_1_to_0", value_str);
            }
            if (plugin->xbar_gain_r_to_r) {
                snprintf(value_str, sizeof(value_str), "%.3f", *plugin->xbar_gain_r_to_r);
                config_set(&plugin->config, "xbar_1_to_1", value_str);
            }
            
            // Save output EQ parameters
            for (int ch = 0; ch < 2; ch++) {
                for (int band = 0; band < NUM_EQ_BANDS; band++) {
                    if (plugin->output_eq_params[ch][band][PARAM_TYPE]) {
                        snprintf(value_str, sizeof(value_str), "output_%d_eq_%d_type", ch, band + 1);
                        char val[16];
                        snprintf(val, sizeof(val), "%d", (int)(*plugin->output_eq_params[ch][band][PARAM_TYPE] + 0.5f));
                        config_set(&plugin->config, value_str, val);
                    }
                    if (plugin->output_eq_params[ch][band][PARAM_FREQ]) {
                        snprintf(value_str, sizeof(value_str), "output_%d_eq_%d_f", ch, band + 1);
                        char val[16];
                        snprintf(val, sizeof(val), "%.1f", *plugin->output_eq_params[ch][band][PARAM_FREQ]);
                        config_set(&plugin->config, value_str, val);
                    }
                    if (plugin->output_eq_params[ch][band][PARAM_Q]) {
                        snprintf(value_str, sizeof(value_str), "output_%d_eq_%d_q", ch, band + 1);
                        char val[16];
                        snprintf(val, sizeof(val), "%.3f", *plugin->output_eq_params[ch][band][PARAM_Q]);
                        config_set(&plugin->config, value_str, val);
                    }
                    if (plugin->output_eq_params[ch][band][PARAM_GAIN]) {
                        snprintf(value_str, sizeof(value_str), "output_%d_eq_%d_gain", ch, band + 1);
                        char val[16];
                        snprintf(val, sizeof(val), "%.1f", *plugin->output_eq_params[ch][band][PARAM_GAIN]);
                        config_set(&plugin->config, value_str, val);
                    }
                }
            }
            
            // Save delay settings
            if (plugin->delay_ms_l) {
                snprintf(value_str, sizeof(value_str), "%.1f", *plugin->delay_ms_l);
                config_set(&plugin->config, "delay_0_ms", value_str);
            }
            if (plugin->delay_ms_r) {
                snprintf(value_str, sizeof(value_str), "%.1f", *plugin->delay_ms_r);
                config_set(&plugin->config, "delay_1_ms", value_str);
            }
            
            // Save gain settings
            if (plugin->input_gain_l_db) {
                snprintf(value_str, sizeof(value_str), "%.1f", *plugin->input_gain_l_db);
                config_set(&plugin->config, "input_gain_0_db", value_str);
            }
            if (plugin->input_gain_r_db) {
                snprintf(value_str, sizeof(value_str), "%.1f", *plugin->input_gain_r_db);
                config_set(&plugin->config, "input_gain_1_db", value_str);
            }
            if (plugin->output_gain_l_db) {
                snprintf(value_str, sizeof(value_str), "%.1f", *plugin->output_gain_l_db);
                config_set(&plugin->config, "output_gain_0_db", value_str);
            }
            if (plugin->output_gain_r_db) {
                snprintf(value_str, sizeof(value_str), "%.1f", *plugin->output_gain_r_db);
                config_set(&plugin->config, "output_gain_1_db", value_str);
            }
            if (plugin->master_volume) {
                snprintf(value_str, sizeof(value_str), "%.1f", *plugin->master_volume);
                config_set(&plugin->config, "master_gain_db", value_str);
            }
            
            // Create directory if it doesn't exist
            if (getuid() == 0) {
                // Root user - save to /etc/ladspa/
                system("mkdir -p /etc/ladspa");
                config_save("/etc/ladspa/speakereq2x2.ini", &plugin->config);
            } else {
                // Normal user - save to ~/.state/ladspa/
                char mkdir_cmd[512];
                snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"$(dirname '%s')\"", plugin->config_path);
                system(mkdir_cmd);
                config_save(plugin->config_path, &plugin->config);
            }
        }
        plugin->last_save_settings_value = save_value;
    }
    
    // Update gain multipliers from control ports (dB to linear conversion)
    if (plugin->input_gain_l_db) {
        float gain_db = *plugin->input_gain_l_db;
        plugin->input_gain_l_mult = powf(10.0f, gain_db / 20.0f);  // dB to linear
    }
    if (plugin->input_gain_r_db) {
        float gain_db = *plugin->input_gain_r_db;
        plugin->input_gain_r_mult = powf(10.0f, gain_db / 20.0f);
    }
    if (plugin->output_gain_l_db) {
        float gain_db = *plugin->output_gain_l_db;
        plugin->output_gain_l_mult = powf(10.0f, gain_db / 20.0f);
    }
    if (plugin->output_gain_r_db) {
        float gain_db = *plugin->output_gain_r_db;
        plugin->output_gain_r_mult = powf(10.0f, gain_db / 20.0f);
    }    
    // Convert master volume from dB to linear multiplier
    if (plugin->master_volume) {
        float gain_db = *plugin->master_volume;
        plugin->master_volume_mult = powf(10.0f, gain_db / 20.0f);
    }    
    // Update input filter coefficients from control ports
    // Left input channel
    for (int band = 0; band < NUM_EQ_BANDS; band++) {
        if (plugin->input_eq_params[0][band][PARAM_TYPE] && 
            plugin->input_eq_params[0][band][PARAM_FREQ] &&
            plugin->input_eq_params[0][band][PARAM_Q] &&
            plugin->input_eq_params[0][band][PARAM_GAIN] &&
            plugin->input_eq_params[0][band][PARAM_ENABLED]) {
            
            int type = (int)(*plugin->input_eq_params[0][band][PARAM_TYPE] + 0.5f);
            float freq = *plugin->input_eq_params[0][band][PARAM_FREQ];
            float q = *plugin->input_eq_params[0][band][PARAM_Q];
            float gain = *plugin->input_eq_params[0][band][PARAM_GAIN];
            int enabled = (int)(*plugin->input_eq_params[0][band][PARAM_ENABLED] + 0.5f);
            
            BiquadCoeffs coeffs;
            if (biquad_calculate(&coeffs, type, plugin->sample_rate, freq, q, gain) == 0) {
                filterbank_set_band(&plugin->input_filterbank_l, band, &coeffs);
            }
            filterbank_set_band_enabled(&plugin->input_filterbank_l, band, enabled);
        }
    }
    
    // Right input channel
    for (int band = 0; band < NUM_EQ_BANDS; band++) {
        if (plugin->input_eq_params[1][band][PARAM_TYPE] && 
            plugin->input_eq_params[1][band][PARAM_FREQ] &&
            plugin->input_eq_params[1][band][PARAM_Q] &&
            plugin->input_eq_params[1][band][PARAM_GAIN] &&
            plugin->input_eq_params[1][band][PARAM_ENABLED]) {
            
            int type = (int)(*plugin->input_eq_params[1][band][PARAM_TYPE] + 0.5f);
            float freq = *plugin->input_eq_params[1][band][PARAM_FREQ];
            float q = *plugin->input_eq_params[1][band][PARAM_Q];
            float gain = *plugin->input_eq_params[1][band][PARAM_GAIN];
            int enabled = (int)(*plugin->input_eq_params[1][band][PARAM_ENABLED] + 0.5f);
            
            BiquadCoeffs coeffs;
            if (biquad_calculate(&coeffs, type, plugin->sample_rate, freq, q, gain) == 0) {
                filterbank_set_band(&plugin->input_filterbank_r, band, &coeffs);
            }
            filterbank_set_band_enabled(&plugin->input_filterbank_r, band, enabled);
        }
    }
    
    // Update output filter coefficients from control ports
    // Left output channel
    for (int band = 0; band < NUM_EQ_BANDS; band++) {
        if (plugin->output_eq_params[0][band][PARAM_TYPE] && 
            plugin->output_eq_params[0][band][PARAM_FREQ] &&
            plugin->output_eq_params[0][band][PARAM_Q] &&
            plugin->output_eq_params[0][band][PARAM_GAIN] &&
            plugin->output_eq_params[0][band][PARAM_ENABLED]) {
            
            int type = (int)(*plugin->output_eq_params[0][band][PARAM_TYPE] + 0.5f);
            float freq = *plugin->output_eq_params[0][band][PARAM_FREQ];
            float q = *plugin->output_eq_params[0][band][PARAM_Q];
            float gain = *plugin->output_eq_params[0][band][PARAM_GAIN];
            int enabled = (int)(*plugin->output_eq_params[0][band][PARAM_ENABLED] + 0.5f);
            
            BiquadCoeffs coeffs;
            if (biquad_calculate(&coeffs, type, plugin->sample_rate, freq, q, gain) == 0) {
                filterbank_set_band(&plugin->output_filterbank_l, band, &coeffs);
            }
            filterbank_set_band_enabled(&plugin->output_filterbank_l, band, enabled);
        }
    }
    
    // Right output channel
    for (int band = 0; band < NUM_EQ_BANDS; band++) {
        if (plugin->output_eq_params[1][band][PARAM_TYPE] && 
            plugin->output_eq_params[1][band][PARAM_FREQ] &&
            plugin->output_eq_params[1][band][PARAM_Q] &&
            plugin->output_eq_params[1][band][PARAM_GAIN] &&
            plugin->output_eq_params[1][band][PARAM_ENABLED]) {
            
            int type = (int)(*plugin->output_eq_params[1][band][PARAM_TYPE] + 0.5f);
            float freq = *plugin->output_eq_params[1][band][PARAM_FREQ];
            float q = *plugin->output_eq_params[1][band][PARAM_Q];
            float gain = *plugin->output_eq_params[1][band][PARAM_GAIN];
            int enabled = (int)(*plugin->output_eq_params[1][band][PARAM_ENABLED] + 0.5f);
            
            BiquadCoeffs coeffs;
            if (biquad_calculate(&coeffs, type, plugin->sample_rate, freq, q, gain) == 0) {
                filterbank_set_band(&plugin->output_filterbank_r, band, &coeffs);
            }
            filterbank_set_band_enabled(&plugin->output_filterbank_r, band, enabled);
        }
    }
    
    // Update crossbar gains from control ports
    if (plugin->xbar_gain_l_to_l) {
        crossbar_set_gain(&plugin->crossbar, 0, 0, *plugin->xbar_gain_l_to_l);
    }
    if (plugin->xbar_gain_l_to_r) {
        crossbar_set_gain(&plugin->crossbar, 1, 0, *plugin->xbar_gain_l_to_r);
    }
    if (plugin->xbar_gain_r_to_l) {
        crossbar_set_gain(&plugin->crossbar, 0, 1, *plugin->xbar_gain_r_to_l);
    }
    if (plugin->xbar_gain_r_to_r) {
        crossbar_set_gain(&plugin->crossbar, 1, 1, *plugin->xbar_gain_r_to_r);
    }
    
    // Allocate temporary buffers for processing stages
    LADSPA_Data input_filtered_l[sample_count];
    LADSPA_Data input_filtered_r[sample_count];
    LADSPA_Data input_gained_l[sample_count];
    LADSPA_Data input_gained_r[sample_count];
    LADSPA_Data crossbar_out_l[sample_count];
    LADSPA_Data crossbar_out_r[sample_count];
    LADSPA_Data delayed_l[sample_count];
    LADSPA_Data delayed_r[sample_count];
    LADSPA_Data output_gained_l[sample_count];
    LADSPA_Data output_gained_r[sample_count];
    
    // Stage 1: Apply input gains
    for (unsigned long i = 0; i < sample_count; i++) {
        input_gained_l[i] = input_l[i] * plugin->input_gain_l_mult;
        input_gained_r[i] = input_r[i] * plugin->input_gain_r_mult;
    }
    
    // Stage 2: Process through input filterbanks
    filterbank_process_buffer(&plugin->input_filterbank_l, input_gained_l, input_filtered_l, sample_count);
    filterbank_process_buffer(&plugin->input_filterbank_r, input_gained_r, input_filtered_r, sample_count);
    
    // Stage 3: Process through crossbar
    const LADSPA_Data *crossbar_inputs[2] = {input_filtered_l, input_filtered_r};
    LADSPA_Data *crossbar_outputs[2] = {crossbar_out_l, crossbar_out_r};
    crossbar_process(&plugin->crossbar, crossbar_inputs, crossbar_outputs, sample_count);
    
    // Stage 4: Process through output filterbanks
    filterbank_process_buffer(&plugin->output_filterbank_l, crossbar_out_l, delayed_l, sample_count);
    filterbank_process_buffer(&plugin->output_filterbank_r, crossbar_out_r, delayed_r, sample_count);
    
    // Update delays only if parameters have changed
    if (plugin->delay_ms_l && plugin->delay_ms_r) {
        float delay_l_ms = *plugin->delay_ms_l;
        float delay_r_ms = *plugin->delay_ms_r;
        
        // Only reinitialize if delay value changed
        if (delay_l_ms != plugin->current_delay_l) {
            delay_free(&plugin->delay_l);
            delay_init(&plugin->delay_l, delay_l_ms, plugin->sample_rate);
            plugin->current_delay_l = delay_l_ms;
        }
        
        if (delay_r_ms != plugin->current_delay_r) {
            delay_free(&plugin->delay_r);
            delay_init(&plugin->delay_r, delay_r_ms, plugin->sample_rate);
            plugin->current_delay_r = delay_r_ms;
        }
    }
    
    // Stage 4: Process through output delays
    delay_process(&plugin->delay_l, delayed_l, output_gained_l, sample_count);
    delay_process(&plugin->delay_r, delayed_r, output_gained_r, sample_count);
    
    // Stage 4b: Apply output gains
    for (unsigned long i = 0; i < sample_count; i++) {
        output_l[i] = output_gained_l[i] * plugin->output_gain_l_mult * plugin->master_volume_mult;
        output_r[i] = output_gained_r[i] * plugin->output_gain_r_mult * plugin->master_volume_mult;
    }
}

static void cleanup_SpeakerEQ2x2(LADSPA_Handle instance) {
    SpeakerEQ2x2 *plugin = (SpeakerEQ2x2 *)instance;
    
    // Free delay resources
    delay_free(&plugin->delay_l);
    delay_free(&plugin->delay_r);
    
    // Free config resources
    config_free(&plugin->config);
    
    // Free config path
    if (plugin->config_path) {
        free(plugin->config_path);
    }
    
    free(instance);
}

static LADSPA_Descriptor *g_speakereq2x2_descriptor = NULL;

const LADSPA_Descriptor *ladspa_descriptor(unsigned long index) {
    if (index != 0) {
        return NULL;
    }
    
    if (g_speakereq2x2_descriptor == NULL) {
        g_speakereq2x2_descriptor = (LADSPA_Descriptor *)malloc(sizeof(LADSPA_Descriptor));
        
        if (g_speakereq2x2_descriptor != NULL) {
            g_speakereq2x2_descriptor->UniqueID = 6840;
            g_speakereq2x2_descriptor->Label = strdup("speakereq2x2");
            g_speakereq2x2_descriptor->Properties = LADSPA_PROPERTY_HARD_RT_CAPABLE;
            g_speakereq2x2_descriptor->Name = strdup("Speaker EQ 2x2 (Stereo)");
            g_speakereq2x2_descriptor->Maker = strdup("HiFiBerry");
            g_speakereq2x2_descriptor->Copyright = strdup("MIT");
            g_speakereq2x2_descriptor->PortCount = TOTAL_PORTS;
            
            LADSPA_PortDescriptor *port_descriptors = 
                (LADSPA_PortDescriptor *)calloc(TOTAL_PORTS, sizeof(LADSPA_PortDescriptor));
            
            // Control port (enable)
            port_descriptors[PORT_ENABLE] = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            
            // Audio ports
            port_descriptors[PORT_INPUT_L] = LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO;
            port_descriptors[PORT_INPUT_R] = LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO;
            port_descriptors[PORT_OUTPUT_L] = LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
            port_descriptors[PORT_OUTPUT_R] = LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
            
            // EQ control ports (input filterbanks)
            for (int i = PORT_CONTROL_BASE; i < PORT_XBAR_BASE; i++) {
                port_descriptors[i] = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            }
            
            // Crossbar control ports
            port_descriptors[PORT_XBAR_L_TO_L] = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            port_descriptors[PORT_XBAR_L_TO_R] = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            port_descriptors[PORT_XBAR_R_TO_L] = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            port_descriptors[PORT_XBAR_R_TO_R] = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            
            // Output EQ control ports (output filterbanks)
            for (int i = PORT_OUTPUT_EQ_BASE; i < PORT_DELAY_BASE; i++) {
                port_descriptors[i] = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            }
            
            // Delay control ports
            port_descriptors[PORT_DELAY_L] = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            port_descriptors[PORT_DELAY_R] = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            
            // Gain control ports
            port_descriptors[PORT_INPUT_GAIN_L] = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            port_descriptors[PORT_INPUT_GAIN_R] = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            port_descriptors[PORT_OUTPUT_GAIN_L] = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            port_descriptors[PORT_OUTPUT_GAIN_R] = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            port_descriptors[PORT_MASTER_VOLUME] = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            port_descriptors[PORT_SAVE_SETTINGS] = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            port_descriptors[PORT_LICENSED] = LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL;
            
            g_speakereq2x2_descriptor->PortDescriptors = port_descriptors;
            
            const char **port_names = (const char **)calloc(TOTAL_PORTS, sizeof(char *));
            port_names[PORT_ENABLE] = strdup("Enable");
            port_names[PORT_INPUT_L] = strdup("Input L");
            port_names[PORT_INPUT_R] = strdup("Input R");
            port_names[PORT_OUTPUT_L] = strdup("Output L");
            port_names[PORT_OUTPUT_R] = strdup("Output R");
            
            // Generate EQ parameter names
            for (int input = 0; input < 2; input++) {
                for (int band = 0; band < NUM_EQ_BANDS; band++) {
                    char name[64];
                    snprintf(name, sizeof(name), "input_%d_eq_%d_type", input, band + 1);
                    port_names[EQ_PORT(input, band, PARAM_TYPE)] = strdup(name);
                    
                    snprintf(name, sizeof(name), "input_%d_eq_%d_f", input, band + 1);
                    port_names[EQ_PORT(input, band, PARAM_FREQ)] = strdup(name);
                    
                    snprintf(name, sizeof(name), "input_%d_eq_%d_q", input, band + 1);
                    port_names[EQ_PORT(input, band, PARAM_Q)] = strdup(name);
                    
                    snprintf(name, sizeof(name), "input_%d_eq_%d_gain", input, band + 1);
                    port_names[EQ_PORT(input, band, PARAM_GAIN)] = strdup(name);
                    
                    snprintf(name, sizeof(name), "input_%d_eq_%d_enabled", input, band + 1);
                    port_names[EQ_PORT(input, band, PARAM_ENABLED)] = strdup(name);
                }
            }
            
            // Crossbar parameter names
            port_names[PORT_XBAR_L_TO_L] = strdup("xbar_0_to_0");
            port_names[PORT_XBAR_L_TO_R] = strdup("xbar_0_to_1");
            port_names[PORT_XBAR_R_TO_L] = strdup("xbar_1_to_0");
            port_names[PORT_XBAR_R_TO_R] = strdup("xbar_1_to_1");
            
            // Generate output EQ parameter names
            for (int output = 0; output < 2; output++) {
                for (int band = 0; band < NUM_EQ_BANDS; band++) {
                    char name[64];
                    snprintf(name, sizeof(name), "output_%d_eq_%d_type", output, band + 1);
                    port_names[OUTPUT_EQ_PORT(output, band, PARAM_TYPE)] = strdup(name);
                    
                    snprintf(name, sizeof(name), "output_%d_eq_%d_f", output, band + 1);
                    port_names[OUTPUT_EQ_PORT(output, band, PARAM_FREQ)] = strdup(name);
                    
                    snprintf(name, sizeof(name), "output_%d_eq_%d_q", output, band + 1);
                    port_names[OUTPUT_EQ_PORT(output, band, PARAM_Q)] = strdup(name);
                    
                    snprintf(name, sizeof(name), "output_%d_eq_%d_gain", output, band + 1);
                    port_names[OUTPUT_EQ_PORT(output, band, PARAM_GAIN)] = strdup(name);
                    
                    snprintf(name, sizeof(name), "output_%d_eq_%d_enabled", output, band + 1);
                    port_names[OUTPUT_EQ_PORT(output, band, PARAM_ENABLED)] = strdup(name);
                }
            }
            
            // Delay parameter names
            port_names[PORT_DELAY_L] = strdup("delay_0_ms");
            port_names[PORT_DELAY_R] = strdup("delay_1_ms");
            
            // Gain parameter names
            port_names[PORT_INPUT_GAIN_L] = strdup("input_gain_0_db");
            port_names[PORT_INPUT_GAIN_R] = strdup("input_gain_1_db");
            port_names[PORT_OUTPUT_GAIN_L] = strdup("output_gain_0_db");
            port_names[PORT_OUTPUT_GAIN_R] = strdup("output_gain_1_db");
            port_names[PORT_MASTER_VOLUME] = strdup("master_gain_db");
            port_names[PORT_SAVE_SETTINGS] = strdup("save_settings");
            port_names[PORT_LICENSED] = strdup("Licensed");
            
            g_speakereq2x2_descriptor->PortNames = port_names;
            
            LADSPA_PortRangeHint *port_range_hints = 
                (LADSPA_PortRangeHint *)calloc(TOTAL_PORTS, sizeof(LADSPA_PortRangeHint));
            
            // Enable control: 0 = off, 1 = on, default 1 (on)
            // LADSPA_HINT_TOGGLED may only be combined with DEFAULT_0 or DEFAULT_1
            port_range_hints[PORT_ENABLE].HintDescriptor = LADSPA_HINT_TOGGLED | LADSPA_HINT_DEFAULT_1;
            
            // Audio ports have no hints
            port_range_hints[PORT_INPUT_L].HintDescriptor = 0;
            port_range_hints[PORT_INPUT_R].HintDescriptor = 0;
            port_range_hints[PORT_OUTPUT_L].HintDescriptor = 0;
            port_range_hints[PORT_OUTPUT_R].HintDescriptor = 0;
            
            // EQ parameter hints
            for (int input = 0; input < 2; input++) {
                for (int band = 0; band < NUM_EQ_BANDS; band++) {
                    // Type: 0-8 (NOOP, HP, LP, PEQ, NOTCH, LS, HS, HP1, LP1)
                    port_range_hints[EQ_PORT(input, band, PARAM_TYPE)].HintDescriptor =
                        LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | 
                        LADSPA_HINT_INTEGER | LADSPA_HINT_DEFAULT_0;
                    port_range_hints[EQ_PORT(input, band, PARAM_TYPE)].LowerBound = 0;
                    port_range_hints[EQ_PORT(input, band, PARAM_TYPE)].UpperBound = 8;
                    
                    // Frequency: 20-20000 Hz
                    port_range_hints[EQ_PORT(input, band, PARAM_FREQ)].HintDescriptor =
                        LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | 
                        LADSPA_HINT_LOGARITHMIC | LADSPA_HINT_DEFAULT_MIDDLE;
                    port_range_hints[EQ_PORT(input, band, PARAM_FREQ)].LowerBound = 20.0f;
                    port_range_hints[EQ_PORT(input, band, PARAM_FREQ)].UpperBound = 20000.0f;
                    
                    // Q: 0.1-10
                    port_range_hints[EQ_PORT(input, band, PARAM_Q)].HintDescriptor =
                        LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | 
                        LADSPA_HINT_DEFAULT_1;
                    port_range_hints[EQ_PORT(input, band, PARAM_Q)].LowerBound = 0.1f;
                    port_range_hints[EQ_PORT(input, band, PARAM_Q)].UpperBound = 10.0f;
                    
                    // Gain: -24 to +24 dB
                    port_range_hints[EQ_PORT(input, band, PARAM_GAIN)].HintDescriptor =
                        LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | 
                        LADSPA_HINT_DEFAULT_0;
                    port_range_hints[EQ_PORT(input, band, PARAM_GAIN)].LowerBound = -24.0f;
                    port_range_hints[EQ_PORT(input, band, PARAM_GAIN)].UpperBound = 24.0f;
                    
                    // Enabled: 0 = off, 1 = on, default 1 (on)
                    port_range_hints[EQ_PORT(input, band, PARAM_ENABLED)].HintDescriptor =
                        LADSPA_HINT_TOGGLED;
                }
            }
            
            // Crossbar gain hints: 0.0 to 2.0, default 1.0 for L->L and R->R, 0.0 for others
            port_range_hints[PORT_XBAR_L_TO_L].HintDescriptor =
                LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_DEFAULT_1;
            port_range_hints[PORT_XBAR_L_TO_L].LowerBound = 0.0f;
            port_range_hints[PORT_XBAR_L_TO_L].UpperBound = 2.0f;
            
            port_range_hints[PORT_XBAR_L_TO_R].HintDescriptor =
                LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_DEFAULT_0;
            port_range_hints[PORT_XBAR_L_TO_R].LowerBound = 0.0f;
            port_range_hints[PORT_XBAR_L_TO_R].UpperBound = 2.0f;
            
            port_range_hints[PORT_XBAR_R_TO_L].HintDescriptor =
                LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_DEFAULT_0;
            port_range_hints[PORT_XBAR_R_TO_L].LowerBound = 0.0f;
            port_range_hints[PORT_XBAR_R_TO_L].UpperBound = 2.0f;
            
            port_range_hints[PORT_XBAR_R_TO_R].HintDescriptor =
                LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_DEFAULT_1;
            port_range_hints[PORT_XBAR_R_TO_R].LowerBound = 0.0f;
            port_range_hints[PORT_XBAR_R_TO_R].UpperBound = 2.0f;
            
            // Output EQ parameter hints (same as input EQ)
            for (int output = 0; output < 2; output++) {
                for (int band = 0; band < NUM_EQ_BANDS; band++) {
                    // Type: 0-8 (NOOP, HP, LP, PEQ, NOTCH, LS, HS, HP1, LP1)
                    port_range_hints[OUTPUT_EQ_PORT(output, band, PARAM_TYPE)].HintDescriptor =
                        LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | 
                        LADSPA_HINT_INTEGER | LADSPA_HINT_DEFAULT_0;
                    port_range_hints[OUTPUT_EQ_PORT(output, band, PARAM_TYPE)].LowerBound = 0;
                    port_range_hints[OUTPUT_EQ_PORT(output, band, PARAM_TYPE)].UpperBound = 8;
                    
                    // Frequency: 20-20000 Hz
                    port_range_hints[OUTPUT_EQ_PORT(output, band, PARAM_FREQ)].HintDescriptor =
                        LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | 
                        LADSPA_HINT_LOGARITHMIC | LADSPA_HINT_DEFAULT_MIDDLE;
                    port_range_hints[OUTPUT_EQ_PORT(output, band, PARAM_FREQ)].LowerBound = 20.0f;
                    port_range_hints[OUTPUT_EQ_PORT(output, band, PARAM_FREQ)].UpperBound = 20000.0f;
                    
                    // Q: 0.1-10
                    port_range_hints[OUTPUT_EQ_PORT(output, band, PARAM_Q)].HintDescriptor =
                        LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | 
                        LADSPA_HINT_DEFAULT_1;
                    port_range_hints[OUTPUT_EQ_PORT(output, band, PARAM_Q)].LowerBound = 0.1f;
                    port_range_hints[OUTPUT_EQ_PORT(output, band, PARAM_Q)].UpperBound = 10.0f;
                    
                    // Gain: -24 to +24 dB
                    port_range_hints[OUTPUT_EQ_PORT(output, band, PARAM_GAIN)].HintDescriptor =
                        LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | 
                        LADSPA_HINT_DEFAULT_0;
                    port_range_hints[OUTPUT_EQ_PORT(output, band, PARAM_GAIN)].LowerBound = -24.0f;
                    port_range_hints[OUTPUT_EQ_PORT(output, band, PARAM_GAIN)].UpperBound = 24.0f;
                    
                    // Enabled: 0 = off, 1 = on, default 1 (on)
                    port_range_hints[OUTPUT_EQ_PORT(output, band, PARAM_ENABLED)].HintDescriptor =
                        LADSPA_HINT_TOGGLED;
                }
            }
            
            // Delay parameter hints: 0-100 ms, default 0
            port_range_hints[PORT_DELAY_L].HintDescriptor =
                LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | 
                LADSPA_HINT_DEFAULT_0;
            port_range_hints[PORT_DELAY_L].LowerBound = 0.0f;
            port_range_hints[PORT_DELAY_L].UpperBound = 100.0f;
            
            port_range_hints[PORT_DELAY_R].HintDescriptor =
                LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | 
                LADSPA_HINT_DEFAULT_0;
            port_range_hints[PORT_DELAY_R].LowerBound = 0.0f;
            port_range_hints[PORT_DELAY_R].UpperBound = 100.0f;
            
            // Gain parameter hints: -24 to +24 dB, default 0
            port_range_hints[PORT_INPUT_GAIN_L].HintDescriptor =
                LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | 
                LADSPA_HINT_DEFAULT_0;
            port_range_hints[PORT_INPUT_GAIN_L].LowerBound = -100.0f;
            port_range_hints[PORT_INPUT_GAIN_L].UpperBound = 24.0f;
            
            port_range_hints[PORT_INPUT_GAIN_R].HintDescriptor =
                LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | 
                LADSPA_HINT_DEFAULT_0;
            port_range_hints[PORT_INPUT_GAIN_R].LowerBound = -100.0f;
            port_range_hints[PORT_INPUT_GAIN_R].UpperBound = 24.0f;
            
            port_range_hints[PORT_OUTPUT_GAIN_L].HintDescriptor =
                LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | 
                LADSPA_HINT_DEFAULT_0;
            port_range_hints[PORT_OUTPUT_GAIN_L].LowerBound = -100.0f;
            port_range_hints[PORT_OUTPUT_GAIN_L].UpperBound = 24.0f;
            
            port_range_hints[PORT_OUTPUT_GAIN_R].HintDescriptor =
                LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | 
                LADSPA_HINT_DEFAULT_0;
            port_range_hints[PORT_OUTPUT_GAIN_R].LowerBound = -100.0f;
            port_range_hints[PORT_OUTPUT_GAIN_R].UpperBound = 24.0f;
            
            port_range_hints[PORT_MASTER_VOLUME].HintDescriptor =
                LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | 
                LADSPA_HINT_DEFAULT_0;
            port_range_hints[PORT_MASTER_VOLUME].LowerBound = -100.0f;
            port_range_hints[PORT_MASTER_VOLUME].UpperBound = 24.0f;
            
            // Save settings trigger: 0 = off, 1 = trigger save
            // A toggled port carries no bounds; the 0..1 range is implied
            port_range_hints[PORT_SAVE_SETTINGS].HintDescriptor =
                LADSPA_HINT_TOGGLED | LADSPA_HINT_DEFAULT_0;
            
            // Licensed output: always 1, retained for API compatibility (read-only output)
            port_range_hints[PORT_LICENSED].HintDescriptor =
                LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_INTEGER;
            port_range_hints[PORT_LICENSED].LowerBound = 0;
            port_range_hints[PORT_LICENSED].UpperBound = 1;
            
            g_speakereq2x2_descriptor->PortRangeHints = port_range_hints;
            
            g_speakereq2x2_descriptor->instantiate = instantiate_SpeakerEQ2x2;
            g_speakereq2x2_descriptor->connect_port = connect_port_SpeakerEQ2x2;
            g_speakereq2x2_descriptor->activate = activate_SpeakerEQ2x2;
            g_speakereq2x2_descriptor->run = run_SpeakerEQ2x2;
            g_speakereq2x2_descriptor->run_adding = NULL;
            g_speakereq2x2_descriptor->set_run_adding_gain = NULL;
            g_speakereq2x2_descriptor->deactivate = NULL;
            g_speakereq2x2_descriptor->cleanup = cleanup_SpeakerEQ2x2;
        }
    }
    
    return g_speakereq2x2_descriptor;
}

void _cleanup(void) __attribute__((destructor));
void _cleanup(void) {
    if (g_speakereq2x2_descriptor != NULL) {
        free((char *)g_speakereq2x2_descriptor->Label);
        free((char *)g_speakereq2x2_descriptor->Name);
        free((char *)g_speakereq2x2_descriptor->Maker);
        free((char *)g_speakereq2x2_descriptor->Copyright);
        free((LADSPA_PortDescriptor *)g_speakereq2x2_descriptor->PortDescriptors);
        for (unsigned long i = 0; i < g_speakereq2x2_descriptor->PortCount; i++) {
            free((char *)g_speakereq2x2_descriptor->PortNames[i]);
        }
        free((char **)g_speakereq2x2_descriptor->PortNames);
        free((LADSPA_PortRangeHint *)g_speakereq2x2_descriptor->PortRangeHints);
        free(g_speakereq2x2_descriptor);
        g_speakereq2x2_descriptor = NULL;
    }
}
