/*
 * biquad.c - Biquad filter coefficient generation
 * 
 * Functions to generate filter coefficients for various filter types
 */

#include <math.h>
#include "biquad.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * Generate Butterworth highpass filter coefficients
 * 
 * @param coeffs Output coefficient structure
 * @param sample_rate Sample rate in Hz
 * @param cutoff_freq Cutoff frequency in Hz
 * @param order Filter order (1 or 2)
 */
void biquad_highpass(BiquadCoeffs *coeffs, LADSPA_Data sample_rate, 
                     LADSPA_Data cutoff_freq, int order) {
    
    LADSPA_Data omega = 2.0 * M_PI * cutoff_freq / sample_rate;
    LADSPA_Data cos_omega = cos(omega);
    LADSPA_Data sin_omega = sin(omega);
    
    if (order == 1) {
        // 1st order highpass using bilinear transform
        // H(s) = s/(s+wc), apply bilinear transform s = 2*fs*(1-z^-1)/(1+z^-1)
        LADSPA_Data c = tan(omega / 2.0);
        LADSPA_Data a0 = 1.0 + c;
        
        coeffs->b0 = 1.0 / a0;
        coeffs->b1 = -1.0 / a0;
        coeffs->b2 = 0.0;
        coeffs->a1 = -(c - 1.0) / a0;  // Note: sign negated for our convention
        coeffs->a2 = 0.0;
        
    } else {
        // 2nd order Butterworth highpass
        // Q = 1/sqrt(2) = 0.7071 for Butterworth
        LADSPA_Data Q = 0.7071067811865476;
        LADSPA_Data alpha = sin_omega / (2.0 * Q);
        
        // Standard biquad form:
        // H(z) = [(1+cos)/2, -(1+cos), (1+cos)/2] / [1+alpha, -2*cos, 1-alpha]
        
        LADSPA_Data a0 = 1.0 + alpha;
        LADSPA_Data a1 = -2.0 * cos_omega;
        LADSPA_Data a2 = 1.0 - alpha;
        
        LADSPA_Data b0 = (1.0 + cos_omega) / 2.0;
        LADSPA_Data b1 = -(1.0 + cos_omega);
        LADSPA_Data b2 = (1.0 + cos_omega) / 2.0;
        
        // Normalize by a0
        coeffs->b0 = b0 / a0;
        coeffs->b1 = b1 / a0;
        coeffs->b2 = b2 / a0;
        coeffs->a1 = -a1 / a0;  // Note: sign negated for our convention
        coeffs->a2 = -a2 / a0;  // Note: sign negated for our convention
    }
}

/**
 * Generate Butterworth lowpass filter coefficients
 * 
 * @param coeffs Output coefficient structure
 * @param sample_rate Sample rate in Hz
 * @param cutoff_freq Cutoff frequency in Hz
 * @param order Filter order (1 or 2)
 */
void biquad_lowpass(BiquadCoeffs *coeffs, LADSPA_Data sample_rate,
                    LADSPA_Data cutoff_freq, int order) {
    
    LADSPA_Data omega = 2.0 * M_PI * cutoff_freq / sample_rate;
    LADSPA_Data cos_omega = cos(omega);
    LADSPA_Data sin_omega = sin(omega);
    
    if (order == 1) {
        // 1st order lowpass using bilinear transform
        // H(s) = wc/(s+wc), apply bilinear transform s = 2*fs*(1-z^-1)/(1+z^-1)
        LADSPA_Data c = tan(omega / 2.0);
        LADSPA_Data a0 = 1.0 + c;
        
        coeffs->b0 = c / a0;
        coeffs->b1 = c / a0;
        coeffs->b2 = 0.0;
        coeffs->a1 = -(c - 1.0) / a0;  // Note: sign negated for our convention
        coeffs->a2 = 0.0;
        
    } else {
        // 2nd order Butterworth lowpass
        LADSPA_Data Q = 0.7071067811865476;
        LADSPA_Data alpha = sin_omega / (2.0 * Q);
        
        LADSPA_Data a0 = 1.0 + alpha;
        LADSPA_Data a1 = -2.0 * cos_omega;
        LADSPA_Data a2 = 1.0 - alpha;
        
        LADSPA_Data b0 = (1.0 - cos_omega) / 2.0;
        LADSPA_Data b1 = 1.0 - cos_omega;
        LADSPA_Data b2 = (1.0 - cos_omega) / 2.0;
        
        coeffs->b0 = b0 / a0;
        coeffs->b1 = b1 / a0;
        coeffs->b2 = b2 / a0;
        coeffs->a1 = -a1 / a0;
        coeffs->a2 = -a2 / a0;
    }
}

/**
 * Generate parametric EQ (peaking) filter coefficients
 * 
 * Based on Audio EQ Cookbook by Robert Bristow-Johnson
 * 
 * @param coeffs Output coefficient structure
 * @param sample_rate Sample rate in Hz
 * @param center_freq Center frequency in Hz
 * @param q Q factor (bandwidth)
 * @param gain_db Gain in dB (positive for boost, negative for cut)
 */
void biquad_peaking(BiquadCoeffs *coeffs, LADSPA_Data sample_rate,
                    LADSPA_Data center_freq, LADSPA_Data q, LADSPA_Data gain_db) {
    
    LADSPA_Data omega = 2.0 * M_PI * center_freq / sample_rate;
    LADSPA_Data cos_omega = cos(omega);
    LADSPA_Data sin_omega = sin(omega);
    LADSPA_Data alpha = sin_omega / (2.0 * q);
    
    // Convert dB gain to linear amplitude
    LADSPA_Data A = pow(10.0, gain_db / 40.0);
    
    // RBJ Audio EQ Cookbook - Peaking/Bell filter
    LADSPA_Data a0 = 1.0 + alpha / A;
    LADSPA_Data a1 = -2.0 * cos_omega;
    LADSPA_Data a2 = 1.0 - alpha / A;
    
    LADSPA_Data b0 = 1.0 + alpha * A;
    LADSPA_Data b1 = -2.0 * cos_omega;
    LADSPA_Data b2 = 1.0 - alpha * A;
    
    // Normalize by a0
    coeffs->b0 = b0 / a0;
    coeffs->b1 = b1 / a0;
    coeffs->b2 = b2 / a0;
    coeffs->a1 = -a1 / a0;  // Note: sign negated for our convention
    coeffs->a2 = -a2 / a0;  // Note: sign negated for our convention
}

/**
 * Generate notch filter coefficients
 * 
 * Based on Audio EQ Cookbook by Robert Bristow-Johnson
 * 
 * @param coeffs Output coefficient structure
 * @param sample_rate Sample rate in Hz
 * @param center_freq Center frequency to notch out in Hz
 * @param q Q factor (narrowness of notch - higher Q = narrower notch)
 */
void biquad_notch(BiquadCoeffs *coeffs, LADSPA_Data sample_rate,
                  LADSPA_Data center_freq, LADSPA_Data q) {
    
    LADSPA_Data omega = 2.0 * M_PI * center_freq / sample_rate;
    LADSPA_Data cos_omega = cos(omega);
    LADSPA_Data sin_omega = sin(omega);
    LADSPA_Data alpha = sin_omega / (2.0 * q);
    
    // RBJ Audio EQ Cookbook - Notch filter
    LADSPA_Data a0 = 1.0 + alpha;
    LADSPA_Data a1 = -2.0 * cos_omega;
    LADSPA_Data a2 = 1.0 - alpha;
    
    LADSPA_Data b0 = 1.0;
    LADSPA_Data b1 = -2.0 * cos_omega;
    LADSPA_Data b2 = 1.0;
    
    // Normalize by a0
    coeffs->b0 = b0 / a0;
    coeffs->b1 = b1 / a0;
    coeffs->b2 = b2 / a0;
    coeffs->a1 = -a1 / a0;  // Note: sign negated for our convention
    coeffs->a2 = -a2 / a0;  // Note: sign negated for our convention
}

/**
 * Generate low-shelf filter coefficients
 * 
 * Based on Audio EQ Cookbook by Robert Bristow-Johnson
 * Boosts or cuts low frequencies below the corner frequency
 * 
 * @param coeffs Output coefficient structure
 * @param sample_rate Sample rate in Hz
 * @param corner_freq Corner frequency in Hz
 * @param q Q factor (slope steepness)
 * @param gain_db Gain in dB (positive for boost, negative for cut)
 */
void biquad_lowshelf(BiquadCoeffs *coeffs, LADSPA_Data sample_rate,
                     LADSPA_Data corner_freq, LADSPA_Data q, LADSPA_Data gain_db) {
    
    LADSPA_Data omega = 2.0 * M_PI * corner_freq / sample_rate;
    LADSPA_Data cos_omega = cos(omega);
    LADSPA_Data sin_omega = sin(omega);
    
    // Convert dB gain to linear amplitude
    LADSPA_Data A = pow(10.0, gain_db / 40.0);
    
    // Using S=1 (slope parameter for shelving filters)
    // alpha = sin(omega)/2 * sqrt((A + 1/A)*(1/S - 1) + 2)
    // For S=1: alpha = sin(omega)/2 * sqrt((A + 1/A) + 2) is invalid
    // Instead use: alpha = sin(omega)/(2*Q)
    LADSPA_Data alpha = sin_omega / (2.0 * q);
    
    // RBJ Audio EQ Cookbook - Low-shelf filter
    LADSPA_Data Ap1 = A + 1.0;
    LADSPA_Data Am1 = A - 1.0;
    LADSPA_Data sqrtA2alpha = 2.0 * sqrt(A) * alpha;
    
    LADSPA_Data a0 =        Ap1 + Am1 * cos_omega + sqrtA2alpha;
    LADSPA_Data a1 = -2.0 * (Am1 + Ap1 * cos_omega);
    LADSPA_Data a2 =        Ap1 + Am1 * cos_omega - sqrtA2alpha;
    
    LADSPA_Data b0 =      A * (Ap1 - Am1 * cos_omega + sqrtA2alpha);
    LADSPA_Data b1 = 2.0 * A * (Am1 - Ap1 * cos_omega);
    LADSPA_Data b2 =      A * (Ap1 - Am1 * cos_omega - sqrtA2alpha);
    
    // Normalize by a0
    coeffs->b0 = b0 / a0;
    coeffs->b1 = b1 / a0;
    coeffs->b2 = b2 / a0;
    coeffs->a1 = -a1 / a0;  // Note: sign negated for our convention
    coeffs->a2 = -a2 / a0;  // Note: sign negated for our convention
}

/**
 * Generate high-shelf filter coefficients
 * 
 * Based on Audio EQ Cookbook by Robert Bristow-Johnson
 * Boosts or cuts high frequencies above the corner frequency
 * 
 * @param coeffs Output coefficient structure
 * @param sample_rate Sample rate in Hz
 * @param corner_freq Corner frequency in Hz
 * @param q Q factor (slope steepness)
 * @param gain_db Gain in dB (positive for boost, negative for cut)
 */
void biquad_highshelf(BiquadCoeffs *coeffs, LADSPA_Data sample_rate,
                      LADSPA_Data corner_freq, LADSPA_Data q, LADSPA_Data gain_db) {
    
    LADSPA_Data omega = 2.0 * M_PI * corner_freq / sample_rate;
    LADSPA_Data cos_omega = cos(omega);
    LADSPA_Data sin_omega = sin(omega);
    
    // Convert dB gain to linear amplitude
    LADSPA_Data A = pow(10.0, gain_db / 40.0);
    
    // alpha = sin(omega)/(2*Q)
    LADSPA_Data alpha = sin_omega / (2.0 * q);
    
    // RBJ Audio EQ Cookbook - High-shelf filter
    LADSPA_Data Ap1 = A + 1.0;
    LADSPA_Data Am1 = A - 1.0;
    LADSPA_Data sqrtA2alpha = 2.0 * sqrt(A) * alpha;
    
    LADSPA_Data a0 =        Ap1 - Am1 * cos_omega + sqrtA2alpha;
    LADSPA_Data a1 =  2.0 * (Am1 - Ap1 * cos_omega);
    LADSPA_Data a2 =        Ap1 - Am1 * cos_omega - sqrtA2alpha;
    
    LADSPA_Data b0 =      A * (Ap1 + Am1 * cos_omega + sqrtA2alpha);
    LADSPA_Data b1 = -2.0 * A * (Am1 + Ap1 * cos_omega);
    LADSPA_Data b2 =      A * (Ap1 + Am1 * cos_omega - sqrtA2alpha);
    
    // Normalize by a0
    coeffs->b0 = b0 / a0;
    coeffs->b1 = b1 / a0;
    coeffs->b2 = b2 / a0;
    coeffs->a1 = -a1 / a0;  // Note: sign negated for our convention
    coeffs->a2 = -a2 / a0;  // Note: sign negated for our convention
}

/**
 * Calculate biquad coefficients based on filter type
 * 
 * @param coeffs Output coefficient structure
 * @param filter_type Filter type constant (FILTER_TYPE_NOOP, FILTER_TYPE_HP, FILTER_TYPE_LP, FILTER_TYPE_PEQ)
 * @param sample_rate Sample rate in Hz
 * @param freq Filter frequency in Hz
 * @param q Q factor (resonance) - used for PEQ, not used for HP/LP Butterworth
 * @param gain Gain in dB - used for PEQ, not used for HP/LP
 * @return 0 on success, -1 on error
 */
int biquad_calculate(BiquadCoeffs *coeffs, int filter_type,
                     LADSPA_Data sample_rate, LADSPA_Data freq,
                     LADSPA_Data q, LADSPA_Data gain) {
    
    if (!coeffs || sample_rate <= 0) {
        return -1;
    }
    
    switch (filter_type) {
        case FILTER_TYPE_NOOP:
            // Passthrough: y[n] = x[n]
            coeffs->b0 = 1.0;
            coeffs->b1 = 0.0;
            coeffs->b2 = 0.0;
            coeffs->a1 = 0.0;
            coeffs->a2 = 0.0;
            break;
            
        case FILTER_TYPE_HP:
            // 2nd order Butterworth highpass
            if (freq <= 0 || freq >= sample_rate / 2.0) {
                return -1;
            }
            biquad_highpass(coeffs, sample_rate, freq, 2);
            break;
            
        case FILTER_TYPE_LP:
            // 2nd order Butterworth lowpass
            if (freq <= 0 || freq >= sample_rate / 2.0) {
                return -1;
            }
            biquad_lowpass(coeffs, sample_rate, freq, 2);
            break;
            
        case FILTER_TYPE_PEQ:
            // Parametric EQ (peaking filter)
            if (freq <= 0 || freq >= sample_rate / 2.0) {
                return -1;
            }
            if (q <= 0) {
                return -1;
            }
            biquad_peaking(coeffs, sample_rate, freq, q, gain);
            break;
            
        case FILTER_TYPE_NOTCH:
            // Notch filter
            if (freq <= 0 || freq >= sample_rate / 2.0) {
                return -1;
            }
            if (q <= 0) {
                return -1;
            }
            biquad_notch(coeffs, sample_rate, freq, q);
            break;
            
        case FILTER_TYPE_LS:
            // Low-shelf filter
            if (freq <= 0 || freq >= sample_rate / 2.0) {
                return -1;
            }
            if (q <= 0) {
                return -1;
            }
            biquad_lowshelf(coeffs, sample_rate, freq, q, gain);
            break;
            
        case FILTER_TYPE_HS:
            // High-shelf filter
            if (freq <= 0 || freq >= sample_rate / 2.0) {
                return -1;
            }
            if (q <= 0) {
                return -1;
            }
            biquad_highshelf(coeffs, sample_rate, freq, q, gain);
            break;
            
        case FILTER_TYPE_HP1:
            // 1st order highpass
            if (freq <= 0 || freq >= sample_rate / 2.0) {
                return -1;
            }
            biquad_highpass(coeffs, sample_rate, freq, 1);
            break;
            
        case FILTER_TYPE_LP1:
            // 1st order lowpass
            if (freq <= 0 || freq >= sample_rate / 2.0) {
                return -1;
            }
            biquad_lowpass(coeffs, sample_rate, freq, 1);
            break;
            
        default:
            // Unknown filter type - set to passthrough
            coeffs->b0 = 1.0;
            coeffs->b1 = 0.0;
            coeffs->b2 = 0.0;
            coeffs->a1 = 0.0;
            coeffs->a2 = 0.0;
            return -1;
    }
    
    return 0;
}
