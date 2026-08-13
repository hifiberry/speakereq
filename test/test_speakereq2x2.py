#!/usr/bin/env python3
"""
Unit tests for speakereq2x2 LADSPA plugin

Tests filter functionality, crossbar routing, and combined scenarios.
"""

import sys
import os
import subprocess
import math

# Add parent directory to path to import test-plugin.py functionality
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# Import precise measurement tool from the same directory
test_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, test_dir)
import measure_response
from measure_response import measure_at_frequencies

class TestResult:
    def __init__(self, name):
        self.name = name
        self.passed = False
        self.message = ""
    
    def pass_test(self, message=""):
        self.passed = True
        self.message = message
    
    def fail_test(self, message):
        self.passed = False
        self.message = message

def run_plugin_test(params, f_min=20, f_max=20000, steps=12):
    """Run test-plugin.py and return frequency response data"""
    parent_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    test_script = os.path.join(parent_dir, 'test-plugin.py')
    
    cmd = [
        sys.executable, test_script, 'speakereq2x2',
        '--params'
    ] + [str(p) for p in params] + [
        '--f-min', str(f_min),
        '--f-max', str(f_max),
        '--steps', str(steps)
    ]
    
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            cwd=parent_dir
        )
        
        if result.returncode != 0:
            print(f"Error: {result.stderr}")
            return None
        
        # Parse response file
        response_file = os.path.join(parent_dir, 'speakereq2x2_response.txt')
        
        data = []
        with open(response_file, 'r') as f:
            lines = f.readlines()
            for line in lines[1:]:  # Skip header
                parts = line.strip().split()
                if len(parts) >= 3:
                    freq = float(parts[0])
                    ch0_db = float(parts[1])
                    ch1_db = float(parts[2])
                    data.append((freq, ch0_db, ch1_db))
        
        return data
    except Exception as e:
        print(f"Error running test: {e}")
        return None

def get_response_at_freq(data, target_freq, tolerance=100):
    """Get response closest to target frequency"""
    best_match = None
    min_diff = float('inf')
    
    for freq, ch0, ch1 in data:
        diff = abs(freq - target_freq)
        if diff < min_diff:
            min_diff = diff
            best_match = (freq, ch0, ch1)
    
    if best_match and min_diff < tolerance:
        return best_match
    return None

def build_params(enable=1.0, input_eq_ch0=None, input_eq_ch1=None, crossbar=None, output_eq_ch0=None, 
                 output_eq_ch1=None, delay_l=0.0, delay_r=0.0, input_gain_l=0.0, 
                 input_gain_r=0.0, output_gain_l=0.0, output_gain_r=0.0, master_volume=0.0,
                 save_settings=0.0):
    """
    Build a complete parameter array for the speakereq2x2 plugin.
    
    Args:
        enable: Enable control (0=disabled/bypass, 1=enabled) (default 1)
        input_eq_ch0: List of 100 params (20 bands × 5 params: type, freq, q, gain, enabled) for input channel 0, or None for NOOP
        input_eq_ch1: List of 100 params (20 bands × 5 params: type, freq, q, gain, enabled) for input channel 1, or None for NOOP
        crossbar: List of 4 params [L->L, L->R, R->L, R->R], or None for identity
        output_eq_ch0: List of 100 params (20 bands × 5 params: type, freq, q, gain, enabled) for output channel 0, or None for NOOP
        output_eq_ch1: List of 100 params (20 bands × 5 params: type, freq, q, gain, enabled) for output channel 1, or None for NOOP
        delay_l: Delay in ms for left channel (default 0)
        delay_r: Delay in ms for right channel (default 0)
        input_gain_l: Input gain in dB for left channel (default 0)
        input_gain_r: Input gain in dB for right channel (default 0)
        output_gain_l: Output gain in dB for left channel (default 0)
        output_gain_r: Output gain in dB for right channel (default 0)
        master_volume: Master volume in dB for all channels (default 0)
        save_settings: Save settings trigger (0=no save, 1=save) (default 0)
    
    Returns:
        List of parameter strings for the plugin
    """
    params = []
    
    # Enable control (port 0)
    params += [str(enable)]
    
    # Input EQ Channel 0 (100 params: 20 bands × 5 params: type, freq, q, gain, enabled)
    if input_eq_ch0 is None:
        params += ['0', '1000', '0.707', '0', '1'] * 20  # NOOP with enabled=1
    else:
        params += [str(p) for p in input_eq_ch0]
    
    # Input EQ Channel 1 (100 params)
    if input_eq_ch1 is None:
        params += ['0', '1000', '0.707', '0', '1'] * 20  # NOOP with enabled=1
    else:
        params += [str(p) for p in input_eq_ch1]
    
    # Crossbar (4 params)
    if crossbar is None:
        params += ['1.0', '0.0', '0.0', '1.0']  # Identity routing
    else:
        params += [str(p) for p in crossbar]
    
    # Output EQ Channel 0 (100 params)
    if output_eq_ch0 is None:
        params += ['0', '1000', '0.707', '0', '1'] * 20  # NOOP with enabled=1
    else:
        params += [str(p) for p in output_eq_ch0]
    
    # Output EQ Channel 1 (100 params)
    if output_eq_ch1 is None:
        params += ['0', '1000', '0.707', '0', '1'] * 20  # NOOP with enabled=1
    else:
        params += [str(p) for p in output_eq_ch1]
    
    # Delay (2 params: L, R in ms)
    params += [str(delay_l), str(delay_r)]
    
    # Gain (4 params: input_l, input_r, output_l, output_r in dB)
    params += [str(input_gain_l), str(input_gain_r), str(output_gain_l), str(output_gain_r)]
    
    # Master volume (1 param in dB)
    params += [str(master_volume)]
    
    # Save settings (1 param: 0=no save, 1=save)
    params += [str(save_settings)]
    
    return params

def test_passthrough():
    """Test that plugin passes audio through with all filters disabled"""
    result = TestResult("Passthrough (NOOP filters)")
    
    # Build parameter array with all NOOP filters
    params = build_params()
    
    # Measure at specific frequencies
    responses = measure_at_frequencies('speakereq2x2', params, [100, 1000, 10000])
    
    if responses is None:
        result.fail_test("Failed to measure frequency response")
        return result
    
    resp_100 = responses.get(100)
    resp_1000 = responses.get(1000)
    resp_10000 = responses.get(10000)
    
    if not all([resp_100, resp_1000, resp_10000]):
        result.fail_test("Missing frequency measurements")
        return result
    
    ch0_100, ch1_100 = resp_100
    ch0_1000, ch1_1000 = resp_1000
    ch0_10000, ch1_10000 = resp_10000
    
    # All should be within 0.5 dB of flat response
    errors = []
    for name, freq, ch0, ch1 in [('100Hz', 100, ch0_100, ch1_100), 
                                   ('1kHz', 1000, ch0_1000, ch1_1000), 
                                   ('10kHz', 10000, ch0_10000, ch1_10000)]:
        if abs(ch0) > 0.5 or abs(ch1) > 0.5:
            errors.append(f"{name}: Ch0={ch0:.2f}dB, Ch1={ch1:.2f}dB")
    
    if errors:
        result.fail_test("Response not flat: " + ", ".join(errors))
    else:
        result.pass_test("All frequencies within 0.5dB of flat")
    
    return result

def test_lowpass_1khz():
    """Test 1kHz lowpass filter on channel 0"""
    result = TestResult("1kHz Lowpass on Channel 0")
    
    # Channel 0 band 1: LP at 1kHz, rest NOOP (all enabled=1)
    input_eq_ch0 = ['2', '1000', '1', '0', '1'] + ['0', '1000', '1', '0', '1'] * 19
    params = build_params(input_eq_ch0=input_eq_ch0)
    
    # Measure at specific frequencies
    responses = measure_at_frequencies('speakereq2x2', params, [100, 1000, 2000])
    
    if responses is None:
        result.fail_test("Failed to measure frequency response")
        return result
    
    resp_100 = responses.get(100)
    resp_1000 = responses.get(1000)
    resp_2000 = responses.get(2000)
    
    if not all([resp_100, resp_1000, resp_2000]):
        result.fail_test("Missing frequency measurements")
        return result
    
    ch0_100, ch1_100 = resp_100
    ch0_1000, ch1_1000 = resp_1000
    ch0_2000, ch1_2000 = resp_2000
    
    # At 1kHz: should be around -3dB (Butterworth cutoff)
    if not (-4 < ch0_1000 < -2):
        result.fail_test(f"At 1kHz: {ch0_1000:.2f}dB (expected ~-3dB)")
        return result
    
    # At 2kHz: should be attenuated more (>-10dB)
    if ch0_2000 > -10:
        result.fail_test(f"At 2kHz: {ch0_2000:.2f}dB (expected <-10dB)")
        return result
    
    # At 100Hz: should be close to 0dB (passband)
    if abs(ch0_100) > 0.5:
        result.fail_test(f"At 100Hz: {ch0_100:.2f}dB (expected ~0dB)")
        return result
    
    # Channel 1 should be unaffected
    if abs(ch1_1000) > 0.5:
        result.fail_test(f"Ch1 affected at 1kHz: {ch1_1000:.2f}dB")
        return result
    
    result.pass_test(f"Cutoff at 1000Hz: {ch0_1000:.2f}dB")
    return result

def test_highpass_1khz():
    """Test 1kHz highpass filter on channel 1"""
    result = TestResult("1kHz Highpass on Channel 1")
    
    # Channel 1 band 1: HP at 1kHz, rest NOOP (all enabled=1)
    input_eq_ch1 = ['1', '1000', '1', '0', '1'] + ['0', '1000', '1', '0', '1'] * 19
    params = build_params(input_eq_ch1=input_eq_ch1)
    
    # Measure at specific frequencies
    responses = measure_at_frequencies('speakereq2x2', params, [100, 1000, 10000])
    
    if responses is None:
        result.fail_test("Failed to measure frequency response")
        return result
    
    resp_100 = responses.get(100)
    resp_1000 = responses.get(1000)
    resp_10000 = responses.get(10000)
    
    if not all([resp_100, resp_1000, resp_10000]):
        result.fail_test("Missing frequency measurements")
        return result
    
    ch0_100, ch1_100 = resp_100
    ch0_1000, ch1_1000 = resp_1000
    ch0_10000, ch1_10000 = resp_10000
    
    # At 1kHz: should be around -3dB
    if not (-4 < ch1_1000 < -2):
        result.fail_test(f"At 1kHz: {ch1_1000:.2f}dB (expected ~-3dB)")
        return result
    
    # At 100Hz: should be heavily attenuated (<-30dB)
    if ch1_100 > -30:
        result.fail_test(f"At 100Hz: {ch1_100:.2f}dB (expected <-30dB)")
        return result
    
    # At 10kHz: should be close to 0dB (passband)
    if abs(ch1_10000) > 0.5:
        result.fail_test(f"At 10kHz: {ch1_10000:.2f}dB (expected ~0dB)")
        return result
    
    # Channel 0 should be unaffected
    if abs(ch0_1000) > 0.5:
        result.fail_test(f"Ch0 affected at 1kHz: {ch0_1000:.2f}dB")
        return result
    
    result.pass_test(f"Cutoff at 1000Hz: {ch1_1000:.2f}dB")
    return result

def test_crossbar_swap():
    """Test crossbar channel swap (L->R, R->L)"""
    result = TestResult("Crossbar Channel Swap")
    
    # Channel 0: LP at 1kHz, Channel 1: HP at 1kHz (all enabled=1)
    input_eq_ch0 = ['2', '1000', '1', '0', '1'] + ['0', '1000', '1', '0', '1'] * 19
    input_eq_ch1 = ['1', '1000', '1', '0', '1'] + ['0', '1000', '1', '0', '1'] * 19
    # Crossbar: swap channels (0->1, 1->0)
    params = build_params(input_eq_ch0=input_eq_ch0, input_eq_ch1=input_eq_ch1, 
                         crossbar=['0', '1', '1', '0'])
    
    # Measure at specific frequencies
    responses = measure_at_frequencies('speakereq2x2', params, [100, 10000])
    
    if responses is None:
        result.fail_test("Failed to measure frequency response")
        return result
    
    resp_100 = responses.get(100)
    resp_10000 = responses.get(10000)
    
    if not all([resp_100, resp_10000]):
        result.fail_test("Missing frequency measurements")
        return result
    
    ch0_100, ch1_100 = resp_100
    ch0_10000, ch1_10000 = resp_10000
    
    # After swap: Ch0 output should have HP (high at 10kHz, low at 100Hz)
    # Ch1 output should have LP (high at 100Hz, low at 10kHz)
    
    # Ch0 should be attenuated at 100Hz (has HP from input 1)
    if ch0_100 > -30:
        result.fail_test(f"Ch0 at 100Hz: {ch0_100:.2f}dB (expected <-30dB for HP)")
        return result
    
    # Ch1 should be flat at 100Hz (has LP from input 0)
    if abs(ch1_100) > 0.5:
        result.fail_test(f"Ch1 at 100Hz: {ch1_100:.2f}dB (expected ~0dB for LP)")
        return result
    
    # Ch0 should be flat at 10kHz (has HP)
    if abs(ch0_10000) > 0.5:
        result.fail_test(f"Ch0 at 10kHz: {ch0_10000:.2f}dB (expected ~0dB for HP)")
        return result
    
    # Ch1 should be attenuated at 10kHz (has LP)
    if ch1_10000 > -30:
        result.fail_test(f"Ch1 at 10kHz: {ch1_10000:.2f}dB (expected <-30dB for LP)")
        return result
    
    result.pass_test("Channels swapped correctly")
    return result

def test_crossbar_mono_sum():
    """Test crossbar mono sum (both inputs to both outputs)"""
    result = TestResult("Crossbar Mono Sum")
    
    # Channel 0: LP at 1kHz, Channel 1: HP at 1kHz (all enabled=1)
    input_eq_ch0 = ['2', '1000', '1', '0', '1'] + ['0', '1000', '1', '0', '1'] * 19
    input_eq_ch1 = ['1', '1000', '1', '0', '1'] + ['0', '1000', '1', '0', '1'] * 19
    # Crossbar: sum both inputs to both outputs (all 0.5)
    params = build_params(input_eq_ch0=input_eq_ch0, input_eq_ch1=input_eq_ch1,
                         crossbar=['0.5', '0.5', '0.5', '0.5'])
    
    # Measure at 1kHz
    responses = measure_at_frequencies('speakereq2x2', params, [1000])
    
    if responses is None:
        result.fail_test("Failed to measure frequency response")
        return result
    
    resp_1000 = responses.get(1000)
    
    if not resp_1000:
        result.fail_test("Missing frequency measurement")
        return result
    
    ch0_1000, ch1_1000 = resp_1000
    
    # Both outputs should be identical (sum of LP+HP)
    if abs(ch0_1000 - ch1_1000) > 1.0:
        result.fail_test(f"Outputs differ: Ch0={ch0_1000:.2f}dB, Ch1={ch1_1000:.2f}dB")
        return result
    
    # At 1kHz, sum of LP+HP should be close to flat (complementary filters)
    # Each is -3dB, so 0.5*(LP+HP) should be around -6dB + some reconstruction
    # This is a rough check
    result.pass_test(f"Both outputs at 1000Hz: {ch0_1000:.2f}dB")
    return result

def test_crossbar_mute():
    """Test crossbar mute (all gains to 0)"""
    result = TestResult("Crossbar Mute")
    
    # Crossbar: all zeros (mute all)
    params = build_params(crossbar=['0', '0', '0', '0'])
    
    # Measure at specific frequencies
    responses = measure_at_frequencies('speakereq2x2', params, [100, 1000, 10000])
    
    if responses is None:
        result.fail_test("Failed to measure frequency response")
        return result
    
    # Check all frequencies - all should be very low (muted)
    for freq in [100, 1000, 10000]:
        resp = responses.get(freq)
        if not resp:
            result.fail_test(f"Missing measurement at {freq}Hz")
            return result
        
        ch0, ch1 = resp
        if ch0 > -80 or ch1 > -80:
            result.fail_test(f"Output not muted at {freq}Hz: Ch0={ch0:.2f}dB, Ch1={ch1:.2f}dB")
            return result
    
    result.pass_test("All outputs muted")
    return result

def test_bandpass_500_2k():
    """Test bandpass filter (500Hz HP + 2kHz LP cascaded)"""
    result = TestResult("Bandpass 500Hz-2kHz")
    
    # Channel 0: HP at 500Hz on band 1, LP at 2kHz on band 2 (all enabled=1)
    input_eq_ch0 = ['1', '500', '1', '0', '1'] + ['2', '2000', '1', '0', '1'] + ['0', '1000', '1', '0', '1'] * 18
    params = build_params(input_eq_ch0=input_eq_ch0)
    
    # Measure at specific frequencies
    responses = measure_at_frequencies('speakereq2x2', params, [100, 1000, 10000])
    
    if responses is None:
        result.fail_test("Failed to measure frequency response")
        return result
    
    resp_100 = responses.get(100)
    resp_1000 = responses.get(1000)
    resp_10000 = responses.get(10000)
    
    if not all([resp_100, resp_1000, resp_10000]):
        result.fail_test("Missing frequency measurements")
        return result
    
    ch0_100, ch1_100 = resp_100
    ch0_1000, ch1_1000 = resp_1000
    ch0_10000, ch1_10000 = resp_10000
    
    # At 100Hz: should be attenuated by HP (below 500Hz cutoff)
    # Expect > -20dB attenuation
    if ch0_100 > -15:
        result.fail_test(f"At 100Hz: {ch0_100:.2f}dB (expected <-15dB, below HP cutoff)")
        return result
    
    # At 1000Hz: should be in passband (between 500Hz and 2kHz)
    # Expect close to 0dB (within -1dB tolerance for cascaded filters)
    if ch0_1000 < -1.5:
        result.fail_test(f"At 1000Hz: {ch0_1000:.2f}dB (expected >-1.5dB, in passband)")
        return result
    
    # At 10000Hz: should be attenuated by LP (above 2kHz cutoff)
    # Expect > -30dB attenuation
    if ch0_10000 > -25:
        result.fail_test(f"At 10kHz: {ch0_10000:.2f}dB (expected <-25dB, above LP cutoff)")
        return result
    
    # Channel 1 should be unaffected
    if abs(ch1_1000) > 0.5:
        result.fail_test(f"Ch1 affected at 1kHz: {ch1_1000:.2f}dB")
        return result
    
    result.pass_test(f"100Hz: {ch0_100:.2f}dB, 1kHz: {ch0_1000:.2f}dB, 10kHz: {ch0_10000:.2f}dB")
    return result

def test_peq_boost_3db():
    """Test PEQ filter with +3dB boost at 1kHz"""
    result = TestResult("PEQ +3dB boost at 1kHz")
    
    # Channel 0, band 0: PEQ at 1kHz, Q=1, +3dB (all enabled=1)
    input_eq_ch0 = ['3', '1000', '1', '3', '1'] + ['0', '1000', '1', '0', '1'] * 19
    params = build_params(input_eq_ch0=input_eq_ch0)
    
    # Measure at exact frequencies: 100Hz, 1000Hz, 10000Hz
    responses = measure_at_frequencies('speakereq2x2', params, [100, 1000, 10000])
    
    if responses is None:
        result.fail_test("Failed to measure frequency response")
        return result
    
    # Check responses
    resp_100 = responses.get(100)
    resp_1000 = responses.get(1000)
    resp_10000 = responses.get(10000)
    
    if not all([resp_100, resp_1000, resp_10000]):
        result.fail_test("Missing frequency measurements")
        return result
    
    ch0_100, ch1_100 = resp_100
    ch0_1000, ch1_1000 = resp_1000
    ch0_10000, ch1_10000 = resp_10000
    
    # At 100Hz: should be close to 0dB (PEQ doesn't affect this frequency much)
    if abs(ch0_100) > 1.0:
        result.fail_test(f"At 100Hz: {ch0_100:.2f}dB (expected ~0dB)")
        return result
    
    # At 1000Hz: should be boosted by ~+3dB
    if ch0_1000 < 2.5 or ch0_1000 > 3.5:
        result.fail_test(f"At 1kHz: {ch0_1000:.2f}dB (expected ~+3dB)")
        return result
    
    # At 10000Hz: should be close to 0dB (PEQ doesn't affect this frequency much)
    if abs(ch0_10000) > 1.0:
        result.fail_test(f"At 10kHz: {ch0_10000:.2f}dB (expected ~0dB)")
        return result
    
    # Channel 1 should be unaffected
    if abs(ch1_1000) > 0.5:
        result.fail_test(f"Ch1 affected at 1kHz: {ch1_1000:.2f}dB")
        return result
    
    result.pass_test(f"100Hz: {ch0_100:.2f}dB, 1kHz: {ch0_1000:.2f}dB, 10kHz: {ch0_10000:.2f}dB")
    return result

def test_peq_cut_20db():
    """Test PEQ filter with -20dB cut at 1kHz"""
    result = TestResult("PEQ -20dB cut at 1kHz")
    
    # Channel 0, band 0: PEQ at 1kHz, Q=1, -20dB (all enabled=1)
    input_eq_ch0 = ['3', '1000', '1', '-20', '1'] + ['0', '1000', '1', '0', '1'] * 19
    params = build_params(input_eq_ch0=input_eq_ch0)
    
    # Measure at exact frequencies: 100Hz, 1000Hz, 10000Hz
    responses = measure_at_frequencies('speakereq2x2', params, [100, 1000, 10000])
    
    if responses is None:
        result.fail_test("Failed to measure frequency response")
        return result
    
    # Check responses
    resp_100 = responses.get(100)
    resp_1000 = responses.get(1000)
    resp_10000 = responses.get(10000)
    
    if not all([resp_100, resp_1000, resp_10000]):
        result.fail_test("Missing frequency measurements")
        return result
    
    ch0_100, ch1_100 = resp_100
    ch0_1000, ch1_1000 = resp_1000
    ch0_10000, ch1_10000 = resp_10000
    
    # At 100Hz: should be close to 0dB (PEQ doesn't affect this frequency much)
    if abs(ch0_100) > 1.0:
        result.fail_test(f"At 100Hz: {ch0_100:.2f}dB (expected ~0dB)")
        return result
    
    # At 1000Hz: should be cut by ~-20dB
    if ch0_1000 > -18.0 or ch0_1000 < -22.0:
        result.fail_test(f"At 1kHz: {ch0_1000:.2f}dB (expected ~-20dB)")
        return result
    
    # At 10000Hz: should be close to 0dB (PEQ doesn't affect this frequency much)
    if abs(ch0_10000) > 1.0:
        result.fail_test(f"At 10kHz: {ch0_10000:.2f}dB (expected ~0dB)")
        return result
    
    # Channel 1 should be unaffected
    if abs(ch1_1000) > 0.5:
        result.fail_test(f"Ch1 affected at 1kHz: {ch1_1000:.2f}dB")
        return result
    
    result.pass_test(f"100Hz: {ch0_100:.2f}dB, 1kHz: {ch0_1000:.2f}dB, 10kHz: {ch0_10000:.2f}dB")
    return result

def test_filter_slots():
    """Test that filters work in different slots (slot 3 on ch1, slot 17 on ch0)"""
    result = TestResult("Filter Slots 3 & 17")
    
    # Channel 0: LP at 1kHz in slot 17 (band index 16) (all enabled=1)
    input_eq_ch0 = ['0', '1000', '1', '0', '1'] * 16 + ['2', '1000', '1', '0', '1'] + ['0', '1000', '1', '0', '1'] * 3
    
    # Channel 1: LP at 1kHz in slot 3 (band index 2) (all enabled=1)
    input_eq_ch1 = ['0', '1000', '1', '0', '1'] * 2 + ['2', '1000', '1', '0', '1'] + ['0', '1000', '1', '0', '1'] * 17
    
    params = build_params(input_eq_ch0=input_eq_ch0, input_eq_ch1=input_eq_ch1)
    
    # Measure at specific frequencies
    responses = measure_at_frequencies('speakereq2x2', params, [100, 1000, 10000])
    
    if responses is None:
        result.fail_test("Failed to measure frequency response")
        return result
    
    resp_100 = responses.get(100)
    resp_1000 = responses.get(1000)
    resp_10000 = responses.get(10000)
    
    if not all([resp_100, resp_1000, resp_10000]):
        result.fail_test("Missing frequency measurements")
        return result
    
    ch0_100, ch1_100 = resp_100
    ch0_1000, ch1_1000 = resp_1000
    ch0_10000, ch1_10000 = resp_10000
    
    # Both channels should have LP response
    # At 1kHz: should be around -3dB (Butterworth cutoff)
    if not (-4 < ch0_1000 < -2):
        result.fail_test(f"Ch0 at 1kHz: {ch0_1000:.2f}dB (expected ~-3dB)")
        return result
    
    if not (-4 < ch1_1000 < -2):
        result.fail_test(f"Ch1 at 1kHz: {ch1_1000:.2f}dB (expected ~-3dB)")
        return result
    
    # At 100Hz: should be close to 0dB (passband)
    if abs(ch0_100) > 0.5:
        result.fail_test(f"Ch0 at 100Hz: {ch0_100:.2f}dB (expected ~0dB)")
        return result
    
    if abs(ch1_100) > 0.5:
        result.fail_test(f"Ch1 at 100Hz: {ch1_100:.2f}dB (expected ~0dB)")
        return result
    
    # At 10kHz: should be attenuated more (>-30dB)
    if ch0_10000 > -25:
        result.fail_test(f"Ch0 at 10kHz: {ch0_10000:.2f}dB (expected <-25dB)")
        return result
    
    if ch1_10000 > -25:
        result.fail_test(f"Ch1 at 10kHz: {ch1_10000:.2f}dB (expected <-25dB)")
        return result
    
    result.pass_test(f"Ch0(slot17): {ch0_1000:.2f}dB, Ch1(slot3): {ch1_1000:.2f}dB at 1kHz")
    return result

def test_notch_1khz():
    """Test notch filter at 1kHz with Q=5"""
    result = TestResult("Notch filter at 1kHz Q=5")
    
    # Channel 0, band 0: NOTCH at 1kHz, Q=5 (all enabled=1)
    input_eq_ch0 = ['4', '1000', '5', '0', '1'] + ['0', '1000', '1', '0', '1'] * 19
    params = build_params(input_eq_ch0=input_eq_ch0)
    
    # Measure at exact frequencies: 100Hz, 1000Hz, 10000Hz
    responses = measure_at_frequencies('speakereq2x2', params, [100, 1000, 10000])
    
    if responses is None:
        result.fail_test("Failed to measure frequency response")
        return result
    
    # Check responses
    resp_100 = responses.get(100)
    resp_1000 = responses.get(1000)
    resp_10000 = responses.get(10000)
    
    if not all([resp_100, resp_1000, resp_10000]):
        result.fail_test("Missing frequency measurements")
        return result
    
    ch0_100, ch1_100 = resp_100
    ch0_1000, ch1_1000 = resp_1000
    ch0_10000, ch1_10000 = resp_10000
    
    # At 100Hz: should be close to 0dB (notch doesn't affect this frequency)
    if abs(ch0_100) > 1.0:
        result.fail_test(f"At 100Hz: {ch0_100:.2f}dB (expected ~0dB)")
        return result
    
    # At 1000Hz: should be deeply attenuated (>-30dB) with Q=5
    if ch0_1000 > -25.0:
        result.fail_test(f"At 1kHz: {ch0_1000:.2f}dB (expected <-25dB for notch)")
        return result
    
    # At 10000Hz: should be close to 0dB (notch doesn't affect this frequency)
    if abs(ch0_10000) > 1.0:
        result.fail_test(f"At 10kHz: {ch0_10000:.2f}dB (expected ~0dB)")
        return result
    
    # Channel 1 should be unaffected
    if abs(ch1_1000) > 0.5:
        result.fail_test(f"Ch1 affected at 1kHz: {ch1_1000:.2f}dB")
        return result
    
    result.pass_test(f"100Hz: {ch0_100:.2f}dB, 1kHz: {ch0_1000:.2f}dB, 10kHz: {ch0_10000:.2f}dB")
    return result

def test_lowshelf_boost_6db():
    """Test low-shelf filter with +6dB boost at 500Hz"""
    result = TestResult("Low-shelf +6dB at 500Hz")
    
    # All bands NOOP except channel 0, band 0: Low-shelf at 500Hz, Q=1, +6dB (all enabled=1)
    input_eq_ch0 = ['5', '500', '1', '6', '1'] + ['0', '1000', '1', '0', '1'] * 19
    params = build_params(input_eq_ch0=input_eq_ch0)
    
    # Measure at exact frequencies: 100Hz, 1000Hz, 10000Hz
    responses = measure_at_frequencies('speakereq2x2', params, [100, 1000, 10000])
    
    if responses is None:
        result.fail_test("Failed to measure frequency response")
        return result
    
    # Check responses
    resp_100 = responses.get(100)
    resp_1000 = responses.get(1000)
    resp_10000 = responses.get(10000)
    
    if not all([resp_100, resp_1000, resp_10000]):
        result.fail_test("Missing frequency measurements")
        return result
    
    ch0_100, ch1_100 = resp_100
    ch0_1000, ch1_1000 = resp_1000
    ch0_10000, ch1_10000 = resp_10000
    
    # At 100Hz: should be boosted close to +6dB (below shelf frequency)
    if ch0_100 < 5.0 or ch0_100 > 7.0:
        result.fail_test(f"At 100Hz: {ch0_100:.2f}dB (expected ~+6dB)")
        return result
    
    # At 1000Hz: should be close to 0dB (above shelf frequency)
    if abs(ch0_1000) > 1.5:
        result.fail_test(f"At 1kHz: {ch0_1000:.2f}dB (expected ~0dB)")
        return result
    
    # At 10000Hz: should be close to 0dB (well above shelf frequency)
    if abs(ch0_10000) > 1.0:
        result.fail_test(f"At 10kHz: {ch0_10000:.2f}dB (expected ~0dB)")
        return result
    
    # Channel 1 should be unaffected
    if abs(ch1_1000) > 0.5:
        result.fail_test(f"Ch1 affected at 1kHz: {ch1_1000:.2f}dB")
        return result
    
    result.pass_test(f"100Hz: {ch0_100:.2f}dB, 1kHz: {ch0_1000:.2f}dB, 10kHz: {ch0_10000:.2f}dB")
    return result

def test_highshelf_cut_6db():
    """Test high-shelf filter with -6dB cut at 5kHz"""
    result = TestResult("High-shelf -6dB at 5kHz")
    
    # All bands NOOP except channel 0, band 0: High-shelf at 5kHz, Q=1, -6dB (all enabled=1)
    input_eq_ch0 = ['6', '5000', '1', '-6', '1'] + ['0', '1000', '1', '0', '1'] * 19
    params = build_params(input_eq_ch0=input_eq_ch0)
    
    # Measure at exact frequencies: 100Hz, 1000Hz, 10000Hz
    responses = measure_at_frequencies('speakereq2x2', params, [100, 1000, 10000])
    
    if responses is None:
        result.fail_test("Failed to measure frequency response")
        return result
    
    # Check responses
    resp_100 = responses.get(100)
    resp_1000 = responses.get(1000)
    resp_10000 = responses.get(10000)
    
    if not all([resp_100, resp_1000, resp_10000]):
        result.fail_test("Missing frequency measurements")
        return result
    
    ch0_100, ch1_100 = resp_100
    ch0_1000, ch1_1000 = resp_1000
    ch0_10000, ch1_10000 = resp_10000
    
    # At 100Hz: should be close to 0dB (below shelf frequency)
    if abs(ch0_100) > 1.0:
        result.fail_test(f"At 100Hz: {ch0_100:.2f}dB (expected ~0dB)")
        return result
    
    # At 1000Hz: should be close to 0dB (below shelf frequency)
    if abs(ch0_1000) > 1.5:
        result.fail_test(f"At 1kHz: {ch0_1000:.2f}dB (expected ~0dB)")
        return result
    
    # At 10000Hz: should be cut close to -6dB (above shelf frequency)
    if ch0_10000 > -5.0 or ch0_10000 < -7.0:
        result.fail_test(f"At 10kHz: {ch0_10000:.2f}dB (expected ~-6dB)")
        return result
    
    # Channel 1 should be unaffected
    if abs(ch1_1000) > 0.5:
        result.fail_test(f"Ch1 affected at 1kHz: {ch1_1000:.2f}dB")
        return result
    
    result.pass_test(f"100Hz: {ch0_100:.2f}dB, 1kHz: {ch0_1000:.2f}dB, 10kHz: {ch0_10000:.2f}dB")
    return result

def test_hp1_1khz():
    """Test 1st order highpass filter at 1kHz"""
    result = TestResult("1st order HP at 1kHz")
    
    # All bands NOOP except channel 1, band 0: HP1 at 1kHz (all enabled=1)
    input_eq_ch1 = ['7', '1000', '1', '0', '1'] + ['0', '1000', '1', '0', '1'] * 19
    params = build_params(input_eq_ch1=input_eq_ch1)
    
    # Measure at specific frequencies
    responses = measure_at_frequencies('speakereq2x2', params, [100, 1000, 10000])
    
    if responses is None:
        result.fail_test("Failed to measure frequency response")
        return result
    
    resp_100 = responses.get(100)
    resp_1000 = responses.get(1000)
    resp_10000 = responses.get(10000)
    
    if not all([resp_100, resp_1000, resp_10000]):
        result.fail_test("Missing frequency measurements")
        return result
    
    ch0_100, ch1_100 = resp_100
    ch0_1000, ch1_1000 = resp_1000
    ch0_10000, ch1_10000 = resp_10000
    
    # At 1kHz: 1st order filter should be around -3dB
    if not (-4 < ch1_1000 < -2):
        result.fail_test(f"At 1kHz: {ch1_1000:.2f}dB (expected ~-3dB)")
        return result
    
    # At 100Hz: should be heavily attenuated but less than 2nd order
    # 1st order: -6dB per octave, so 100Hz is ~3.3 octaves below 1kHz = ~-20dB
    if ch1_100 > -15:
        result.fail_test(f"At 100Hz: {ch1_100:.2f}dB (expected <-15dB)")
        return result
    
    # At 10kHz: should be close to 0dB (passband)
    if abs(ch1_10000) > 0.5:
        result.fail_test(f"At 10kHz: {ch1_10000:.2f}dB (expected ~0dB)")
        return result
    
    # Channel 0 should be unaffected
    if abs(ch0_1000) > 0.5:
        result.fail_test(f"Ch0 affected at 1kHz: {ch0_1000:.2f}dB")
        return result
    
    result.pass_test(f"Cutoff at 1000Hz: {ch1_1000:.2f}dB")
    return result

def test_lp1_1khz():
    """Test 1st order lowpass filter at 1kHz"""
    result = TestResult("1st order LP at 1kHz")
    
    # All bands NOOP except channel 1, band 0: LP1 at 1kHz (all enabled=1)
    input_eq_ch1 = ['8', '1000', '1', '0', '1'] + ['0', '1000', '1', '0', '1'] * 19
    params = build_params(input_eq_ch1=input_eq_ch1)
    
    # Measure at specific frequencies
    responses = measure_at_frequencies('speakereq2x2', params, [100, 1000, 10000])
    
    if responses is None:
        result.fail_test("Failed to measure frequency response")
        return result
    
    resp_100 = responses.get(100)
    resp_1000 = responses.get(1000)
    resp_10000 = responses.get(10000)
    
    if not all([resp_100, resp_1000, resp_10000]):
        result.fail_test("Missing frequency measurements")
        return result
    
    ch0_100, ch1_100 = resp_100
    ch0_1000, ch1_1000 = resp_1000
    ch0_10000, ch1_10000 = resp_10000
    
    # At 1kHz: 1st order filter should be around -3dB
    if not (-4 < ch1_1000 < -2):
        result.fail_test(f"At 1kHz: {ch1_1000:.2f}dB (expected ~-3dB)")
        return result
    
    # At 100Hz: should be close to 0dB (passband)
    if abs(ch1_100) > 0.5:
        result.fail_test(f"At 100Hz: {ch1_100:.2f}dB (expected ~0dB)")
        return result
    
    # At 10kHz: should be attenuated but less than 2nd order
    # 1st order: -6dB per octave, so 10kHz is ~3.3 octaves above 1kHz = ~-20dB
    if ch1_10000 > -15:
        result.fail_test(f"At 10kHz: {ch1_10000:.2f}dB (expected <-15dB)")
        return result
    
    # Channel 0 should be unaffected
    if abs(ch0_1000) > 0.5:
        result.fail_test(f"Ch0 affected at 1kHz: {ch0_1000:.2f}dB")
        return result
    
    result.pass_test(f"Cutoff at 1000Hz: {ch1_1000:.2f}dB")
    return result

def test_all_filter_types():
    """Test all filter types working together in the same filterbank"""
    result = TestResult("All filter types combined")
    
    # Channel 0: Use all 8 filter types across different bands
    # Band 0: HP at 100Hz (type=1)
    # Band 1: LP at 10kHz (type=2)
    # Band 2: PEQ +3dB at 500Hz Q=2 (type=3)
    # Band 3: NOTCH at 2kHz Q=5 (type=4)
    # Band 4: LS +3dB at 200Hz (type=5)
    # Band 5: HS -3dB at 8kHz (type=6)
    # Band 6: HP1 at 150Hz (type=7)
    # Band 7: LP1 at 8kHz (type=8)
    # Bands 8-19: NOOP
    input_eq_ch0 = (
        ['1', '100', '1', '0', '1'] +    # Band 0: HP 100Hz (enabled)
        ['2', '10000', '1', '0', '1'] +  # Band 1: LP 10kHz (enabled)
        ['3', '500', '2', '3', '1'] +    # Band 2: PEQ +3dB at 500Hz (enabled)
        ['4', '2000', '5', '0', '1'] +   # Band 3: Notch at 2kHz (enabled)
        ['5', '200', '1', '3', '1'] +    # Band 4: LS +3dB at 200Hz (enabled)
        ['6', '8000', '1', '-3', '1'] +  # Band 5: HS -3dB at 8kHz (enabled)
        ['7', '150', '1', '0', '1'] +    # Band 6: HP1 at 150Hz (enabled)
        ['8', '8000', '1', '0', '1'] +   # Band 7: LP1 at 8kHz (enabled)
        ['0', '1000', '1', '0', '1'] * 12  # Bands 8-19: NOOP (enabled)
    )
    params = build_params(input_eq_ch0=input_eq_ch0)
    
    # Measure at various frequencies to check filter interactions
    responses = measure_at_frequencies('speakereq2x2', params, [50, 150, 500, 2000, 8000, 12000])
    
    if responses is None:
        result.fail_test("Failed to measure frequency response")
        return result
    
    resp_50 = responses.get(50)
    resp_150 = responses.get(150)
    resp_500 = responses.get(500)
    resp_2000 = responses.get(2000)
    resp_8000 = responses.get(8000)
    resp_12000 = responses.get(12000)
    
    if not all([resp_50, resp_150, resp_500, resp_2000, resp_8000, resp_12000]):
        result.fail_test("Missing frequency measurements")
        return result
    
    ch0_50, ch1_50 = resp_50
    ch0_150, ch1_150 = resp_150
    ch0_500, ch1_500 = resp_500
    ch0_2000, ch1_2000 = resp_2000
    ch0_8000, ch1_8000 = resp_8000
    ch0_12000, ch1_12000 = resp_12000
    
    # At 50Hz: heavily attenuated by HP (100Hz) and HP1 (150Hz)
    if ch0_50 > -18:
        result.fail_test(f"At 50Hz: {ch0_50:.2f}dB (expected <-18dB, HP filters active)")
        return result
    
    # At 150Hz: partial attenuation from HP at 100Hz, but low-shelf should boost
    # Complex interaction - just verify it's not completely attenuated
    if ch0_150 < -15:
        result.fail_test(f"At 150Hz: {ch0_150:.2f}dB (expected >-15dB)")
        return result
    
    # At 500Hz: PEQ boost (+3dB) plus some low-shelf boost
    if ch0_500 < 2.0:
        result.fail_test(f"At 500Hz: {ch0_500:.2f}dB (expected >+2dB from PEQ+LS)")
        return result
    
    # At 2000Hz: notch should heavily attenuate
    if ch0_2000 > -20:
        result.fail_test(f"At 2kHz: {ch0_2000:.2f}dB (expected <-20dB from notch)")
        return result
    
    # At 8000Hz: high-shelf cut (-3dB) and LP1 starting to roll off
    if ch0_8000 > -2 or ch0_8000 < -6:
        result.fail_test(f"At 8kHz: {ch0_8000:.2f}dB (expected -2 to -6dB from HS+LP1)")
        return result
    
    # At 12000Hz: LP at 10kHz should attenuate, plus HS cut and LP1 rolloff
    if ch0_12000 > -8:
        result.fail_test(f"At 12kHz: {ch0_12000:.2f}dB (expected <-8dB from LP+HS+LP1)")
        return result
    
    # Channel 1 should be unaffected at all frequencies
    for freq, (_, ch1_resp) in responses.items():
        if abs(ch1_resp) > 0.5:
            result.fail_test(f"Ch1 affected at {freq}Hz: {ch1_resp:.2f}dB")
            return result
    
    result.pass_test(f"50Hz: {ch0_50:.2f}dB, 500Hz: {ch0_500:.2f}dB, 2kHz: {ch0_2000:.2f}dB, 8kHz: {ch0_8000:.2f}dB")
    return result

def test_output_lowpass_1khz():
    """Test lowpass filter at 1kHz on output channel"""
    result = TestResult("Output LP at 1kHz")
    
    # Output channel 1 (right): LP at 1kHz on band 0 (all enabled=1)
    output_eq_ch1 = ['2', '1000', '1', '0', '1'] + ['0', '1000', '1', '0', '1'] * 19
    params = build_params(output_eq_ch1=output_eq_ch1)
    
    # Measure at specific frequencies
    responses = measure_at_frequencies('speakereq2x2', params, [100, 1000, 10000])
    
    if responses is None:
        result.fail_test("Failed to measure frequency response")
        return result
    
    resp_100 = responses.get(100)
    resp_1000 = responses.get(1000)
    resp_10000 = responses.get(10000)
    
    if not all([resp_100, resp_1000, resp_10000]):
        result.fail_test("Missing frequency measurements")
        return result
    
    ch0_100, ch1_100 = resp_100
    ch0_1000, ch1_1000 = resp_1000
    ch0_10000, ch1_10000 = resp_10000
    
    # At 1kHz: should be around -3dB
    if not (-4 < ch1_1000 < -2):
        result.fail_test(f"At 1kHz: {ch1_1000:.2f}dB (expected ~-3dB)")
        return result
    
    # At 100Hz: should be close to 0dB (passband)
    if abs(ch1_100) > 0.5:
        result.fail_test(f"At 100Hz: {ch1_100:.2f}dB (expected ~0dB)")
        return result
    
    # At 10kHz: should be heavily attenuated (<-30dB)
    if ch1_10000 > -30:
        result.fail_test(f"At 10kHz: {ch1_10000:.2f}dB (expected <-30dB)")
        return result
    
    # Channel 0 should be unaffected
    if abs(ch0_1000) > 0.5:
        result.fail_test(f"Ch0 affected at 1kHz: {ch0_1000:.2f}dB")
        return result
    
    result.pass_test(f"Output LP Cutoff at 1000Hz: {ch1_1000:.2f}dB")
    return result

def test_output_peq_boost_3db():
    """Test output PEQ filter with +3dB boost at 1kHz"""
    result = TestResult("Output PEQ +3dB boost at 1kHz")
    
    # Output channel 0: PEQ at 1kHz, Q=1, +3dB on band 0 (all enabled=1)
    output_eq_ch0 = ['3', '1000', '1', '3', '1'] + ['0', '1000', '1', '0', '1'] * 19
    params = build_params(output_eq_ch0=output_eq_ch0)
    
    # Measure at exact frequencies
    responses = measure_at_frequencies('speakereq2x2', params, [100, 1000, 10000])
    
    if responses is None:
        result.fail_test("Failed to measure frequency response")
        return result
    
    resp_100 = responses.get(100)
    resp_1000 = responses.get(1000)
    resp_10000 = responses.get(10000)
    
    if not all([resp_100, resp_1000, resp_10000]):
        result.fail_test("Missing frequency measurements")
        return result
    
    ch0_100, ch1_100 = resp_100
    ch0_1000, ch1_1000 = resp_1000
    ch0_10000, ch1_10000 = resp_10000
    
    # At 100Hz: should be close to 0dB
    if abs(ch0_100) > 1.0:
        result.fail_test(f"At 100Hz: {ch0_100:.2f}dB (expected ~0dB)")
        return result
    
    # At 1000Hz: should be boosted by ~+3dB
    if ch0_1000 < 2.5 or ch0_1000 > 3.5:
        result.fail_test(f"At 1kHz: {ch0_1000:.2f}dB (expected ~+3dB)")
        return result
    
    # At 10000Hz: should be close to 0dB
    if abs(ch0_10000) > 1.0:
        result.fail_test(f"At 10kHz: {ch0_10000:.2f}dB (expected ~0dB)")
        return result
    
    # Channel 1 should be unaffected
    if abs(ch1_1000) > 0.5:
        result.fail_test(f"Ch1 affected at 1kHz: {ch1_1000:.2f}dB")
        return result
    
    result.pass_test(f"Output PEQ: 100Hz: {ch0_100:.2f}dB, 1kHz: {ch0_1000:.2f}dB, 10kHz: {ch0_10000:.2f}dB")
    return result

def test_delay_with_zero_ms():
    """Test that 0ms delay does not affect frequency response"""
    result = TestResult("Delay with 0ms (passthrough)")
    
    # Use build_params with defaults (all NOOP, 0ms delay, 0dB gains)
    params = build_params()
    
    # Test at various frequencies
    test_freqs = [100, 500, 1000, 5000, 10000]
    measurements = measure_at_frequencies('speakereq2x2', params, test_freqs)
    
    if measurements is None:
        result.fail_test("Failed to measure frequency response")
        return result
    
    # Check that all frequencies are within 0.1 dB of 0 dB
    all_passed = True
    max_deviation = 0.0
    
    for freq in test_freqs:
        ch0_db, ch1_db = measurements[freq]
        deviation_l = abs(ch0_db)
        deviation_r = abs(ch1_db)
        max_deviation = max(max_deviation, deviation_l, deviation_r)
        
        if deviation_l > 0.1 or deviation_r > 0.1:
            all_passed = False
            result.fail_test(f"Frequency response affected at {freq}Hz: L={ch0_db:.2f}dB, R={ch1_db:.2f}dB")
            return result
    
    if all_passed:
        result.pass_test(f"Frequency response unchanged (max deviation: {max_deviation:.3f}dB)")
    
    return result

def test_delay_with_20ms():
    """Test that 20ms delay does not affect frequency response
    
    Note: A 20ms delay means the first 20ms of output will be silence (zeros from the
    delay buffer initialization). For short test signals, this causes a slight RMS
    reduction in the measurement window, resulting in a small negative dB reading.
    This is expected behavior and we use a relaxed 0.5dB threshold instead of 0.1dB.
    """
    result = TestResult("Delay with 20ms (passthrough)")
    
    # Use build_params with 20ms delay on both channels
    params = build_params(delay_l=20.0, delay_r=20.0)
    
    # Test at various frequencies
    test_freqs = [100, 500, 1000, 5000, 10000]
    measurements = measure_at_frequencies('speakereq2x2', params, test_freqs)
    
    if measurements is None:
        result.fail_test("Failed to measure frequency response")
        return result
    
    # Check that all frequencies are within 0.5 dB of 0 dB
    # Note: 20ms delay causes initial silence in output, affecting short signal measurements
    all_passed = True
    max_deviation = 0.0
    
    for freq in test_freqs:
        ch0_db, ch1_db = measurements[freq]
        deviation_l = abs(ch0_db)
        deviation_r = abs(ch1_db)
        max_deviation = max(max_deviation, deviation_l, deviation_r)
        
        if deviation_l > 0.5 or deviation_r > 0.5:
            all_passed = False
            result.fail_test(f"Frequency response affected at {freq}Hz: L={ch0_db:.2f}dB, R={ch1_db:.2f}dB")
            return result
    
    if all_passed:
        result.pass_test(f"Frequency response unchanged with 20ms delay (max deviation: {max_deviation:.3f}dB)")
    
    return result

def test_input_gain_6db():
    """Test +6dB input gain boost"""
    result = TestResult("Input Gain +6dB boost")
    
    # Use build_params with +6dB input gain on both channels
    params = build_params(input_gain_l=6.0, input_gain_r=6.0)
    
    # Test at various frequencies
    test_freqs = [100, 500, 1000, 5000, 10000]
    measurements = measure_at_frequencies('speakereq2x2', params, test_freqs)
    
    if measurements is None:
        result.fail_test("Failed to measure frequency response")
        return result
    
    # Check that all frequencies have approximately +6dB gain
    # Allow 0.5dB tolerance
    all_passed = True
    
    for freq in test_freqs:
        ch0_db, ch1_db = measurements[freq]
        
        # Expected: ~6dB gain
        expected_gain = 6.0
        tolerance = 0.5
        
        deviation_l = abs(ch0_db - expected_gain)
        deviation_r = abs(ch1_db - expected_gain)
        
        if deviation_l > tolerance or deviation_r > tolerance:
            all_passed = False
            result.fail_test(f"Gain mismatch at {freq}Hz: L={ch0_db:.2f}dB (expected ~{expected_gain}dB), R={ch1_db:.2f}dB (expected ~{expected_gain}dB)")
            return result
    
    if all_passed:
        result.pass_test(f"Input gain +6dB applied correctly on both channels")
    
    return result

def test_input_gain_minus_12db():
    """Test -12dB input gain cut"""
    result = TestResult("Input Gain -12dB cut")
    
    # Use build_params with -12dB input gain on both channels
    params = build_params(input_gain_l=-12.0, input_gain_r=-12.0)
    
    # Test at various frequencies
    test_freqs = [100, 500, 1000, 5000, 10000]
    measurements = measure_at_frequencies('speakereq2x2', params, test_freqs)
    
    if measurements is None:
        result.fail_test("Failed to measure frequency response")
        return result
    
    # Check that all frequencies have approximately -12dB gain
    all_passed = True
    
    for freq in test_freqs:
        ch0_db, ch1_db = measurements[freq]
        
        expected_gain = -12.0
        tolerance = 0.5
        
        deviation_l = abs(ch0_db - expected_gain)
        deviation_r = abs(ch1_db - expected_gain)
        
        if deviation_l > tolerance or deviation_r > tolerance:
            all_passed = False
            result.fail_test(f"Gain mismatch at {freq}Hz: L={ch0_db:.2f}dB (expected ~{expected_gain}dB), R={ch1_db:.2f}dB (expected ~{expected_gain}dB)")
            return result
    
    if all_passed:
        result.pass_test(f"Input gain -12dB applied correctly on both channels")
    
    return result

def test_output_gain_3db():
    """Test +3dB output gain boost"""
    result = TestResult("Output Gain +3dB boost")
    
    # Use build_params with +3dB output gain on both channels
    params = build_params(output_gain_l=3.0, output_gain_r=3.0)
    
    # Test at various frequencies
    test_freqs = [100, 500, 1000, 5000, 10000]
    measurements = measure_at_frequencies('speakereq2x2', params, test_freqs)
    
    if measurements is None:
        result.fail_test("Failed to measure frequency response")
        return result
    
    # Check that all frequencies have approximately +3dB gain
    all_passed = True
    
    for freq in test_freqs:
        ch0_db, ch1_db = measurements[freq]
        
        expected_gain = 3.0
        tolerance = 0.5
        
        deviation_l = abs(ch0_db - expected_gain)
        deviation_r = abs(ch1_db - expected_gain)
        
        if deviation_l > tolerance or deviation_r > tolerance:
            all_passed = False
            result.fail_test(f"Gain mismatch at {freq}Hz: L={ch0_db:.2f}dB (expected ~{expected_gain}dB), R={ch1_db:.2f}dB (expected ~{expected_gain}dB)")
            return result
    
    if all_passed:
        result.pass_test(f"Output gain +3dB applied correctly on both channels")
    
    return result

def test_combined_input_output_gain():
    """Test combined input and output gain"""
    result = TestResult("Combined Input +6dB + Output -3dB")
    
    # Use build_params with +6dB input and -3dB output gains
    params = build_params(input_gain_l=6.0, input_gain_r=6.0, output_gain_l=-3.0, output_gain_r=-3.0)
    
    # Test at various frequencies
    test_freqs = [100, 500, 1000, 5000, 10000]
    measurements = measure_at_frequencies('speakereq2x2', params, test_freqs)
    
    if measurements is None:
        result.fail_test("Failed to measure frequency response")
        return result
    
    # Check that net gain is +6 - 3 = +3 dB
    all_passed = True
    
    for freq in test_freqs:
        ch0_db, ch1_db = measurements[freq]
        
        expected_gain = 3.0  # +6dB input - 3dB output = +3dB net
        tolerance = 0.5
        
        deviation_l = abs(ch0_db - expected_gain)
        deviation_r = abs(ch1_db - expected_gain)
        
        if deviation_l > tolerance or deviation_r > tolerance:
            all_passed = False
            result.fail_test(f"Combined gain mismatch at {freq}Hz: L={ch0_db:.2f}dB (expected ~{expected_gain}dB), R={ch1_db:.2f}dB (expected ~{expected_gain}dB)")
            return result
    
    if all_passed:
        result.pass_test(f"Combined gain (+6dB input, -3dB output = +3dB net) applied correctly")
    
    return result

def test_independent_channel_gains():
    """Test different gains on left and right channels"""
    result = TestResult("Independent channel gains (L: +6dB, R: -6dB)")
    
    # Use build_params with different gains for each channel
    params = build_params(input_gain_l=6.0, input_gain_r=-6.0)
    
    # Test at various frequencies
    test_freqs = [100, 500, 1000, 5000, 10000]
    measurements = measure_at_frequencies('speakereq2x2', params, test_freqs)
    
    if measurements is None:
        result.fail_test("Failed to measure frequency response")
        return result
    
    # Check that left channel has +6dB and right has -6dB
    all_passed = True
    
    for freq in test_freqs:
        ch0_db, ch1_db = measurements[freq]
        
        expected_l = 6.0
        expected_r = -6.0
        tolerance = 0.5
        
        deviation_l = abs(ch0_db - expected_l)
        deviation_r = abs(ch1_db - expected_r)
        
        if deviation_l > tolerance or deviation_r > tolerance:
            all_passed = False
            result.fail_test(f"Channel gain mismatch at {freq}Hz: L={ch0_db:.2f}dB (expected ~{expected_l}dB), R={ch1_db:.2f}dB (expected ~{expected_r}dB)")
            return result
    
    if all_passed:
        result.pass_test(f"Independent channel gains applied correctly (L: +6dB, R: -6dB)")
    
    return result

def test_master_gain_10db():
    """Test +10dB master gain boost"""
    result = TestResult("Master Gain +10dB boost")
    
    # Use build_params with +10dB master volume on both channels
    params = build_params(master_volume=10.0)
    
    # Test at various frequencies
    test_freqs = [100, 500, 1000, 5000, 10000]
    measurements = measure_at_frequencies('speakereq2x2', params, test_freqs)
    
    if measurements is None:
        result.fail_test("Failed to measure frequency response")
        return result
    
    # Check that all frequencies have approximately +10dB gain
    # Allow 0.5dB tolerance
    all_passed = True
    
    for freq in test_freqs:
        ch0_db, ch1_db = measurements[freq]
        
        # Expected: ~10dB gain
        expected_gain = 10.0
        tolerance = 0.5
        
        deviation_l = abs(ch0_db - expected_gain)
        deviation_r = abs(ch1_db - expected_gain)
        
        if deviation_l > tolerance or deviation_r > tolerance:
            all_passed = False
            result.fail_test(f"Gain mismatch at {freq}Hz: L={ch0_db:.2f}dB (expected ~{expected_gain}dB), R={ch1_db:.2f}dB (expected ~{expected_gain}dB)")
            return result
    
    if all_passed:
        result.pass_test(f"Master gain +10dB applied correctly on both channels")
    
    return result

def test_master_gain_minus_6db():
    """Test -6dB master gain cut"""
    result = TestResult("Master Gain -6dB cut")
    
    # Use build_params with -6dB master volume on both channels
    params = build_params(master_volume=-6.0)
    
    # Test at various frequencies
    test_freqs = [100, 500, 1000, 5000, 10000]
    measurements = measure_at_frequencies('speakereq2x2', params, test_freqs)
    
    if measurements is None:
        result.fail_test("Failed to measure frequency response")
        return result
    
    # Check that all frequencies have approximately -6dB gain
    all_passed = True
    
    for freq in test_freqs:
        ch0_db, ch1_db = measurements[freq]
        
        expected_gain = -6.0
        tolerance = 0.5
        
        deviation_l = abs(ch0_db - expected_gain)
        deviation_r = abs(ch1_db - expected_gain)
        
        if deviation_l > tolerance or deviation_r > tolerance:
            all_passed = False
            result.fail_test(f"Gain mismatch at {freq}Hz: L={ch0_db:.2f}dB (expected ~{expected_gain}dB), R={ch1_db:.2f}dB (expected ~{expected_gain}dB)")
            return result
    
    if all_passed:
        result.pass_test(f"Master gain -6dB applied correctly on both channels")
    
    return result

def test_master_gain_with_output_gain():
    """Test master gain combined with output gain (cascaded)"""
    result = TestResult("Master Gain +6dB with Output Gain +3dB")
    
    # Use build_params with +3dB output gain AND +6dB master volume
    # Expected result: +9dB total (3dB + 6dB)
    params = build_params(output_gain_l=3.0, output_gain_r=3.0, master_volume=6.0)
    
    # Test at various frequencies
    test_freqs = [100, 500, 1000, 5000, 10000]
    measurements = measure_at_frequencies('speakereq2x2', params, test_freqs)
    
    if measurements is None:
        result.fail_test("Failed to measure frequency response")
        return result
    
    # Check that all frequencies have approximately +9dB gain (3dB output + 6dB master)
    all_passed = True
    
    for freq in test_freqs:
        ch0_db, ch1_db = measurements[freq]
        
        expected_gain = 9.0  # 3dB + 6dB
        tolerance = 0.5
        
        deviation_l = abs(ch0_db - expected_gain)
        deviation_r = abs(ch1_db - expected_gain)
        
        if deviation_l > tolerance or deviation_r > tolerance:
            all_passed = False
            result.fail_test(f"Gain mismatch at {freq}Hz: L={ch0_db:.2f}dB (expected ~{expected_gain}dB), R={ch1_db:.2f}dB (expected ~{expected_gain}dB)")
            return result
    
    if all_passed:
        result.pass_test(f"Master gain (+6dB) cascades correctly with output gain (+3dB) = +9dB total")
    
    return result

def test_enable_disable_with_filter():
    """Test that per-band enabled parameter bypasses processing when disabled"""
    result = TestResult("Per-band Enable/Disable with filter")
    
    # Test 1: Create a lowpass filter at 1kHz on input channel 0 with enabled=1
    input_eq_ch0_enabled = ['2', '1000', '0.707', '0', '1'] + ['0', '1000', '0.707', '0', '1'] * 19
    params_enabled = build_params(input_eq_ch0=input_eq_ch0_enabled)
    responses_enabled = measure_at_frequencies('speakereq2x2', params_enabled, [100, 1000, 10000])
    
    if responses_enabled is None:
        result.fail_test("Failed to measure frequency response with enabled=1")
        return result
    
    # Test 2: Same lowpass filter but with enabled=0 (bypassed)
    input_eq_ch0_disabled = ['2', '1000', '0.707', '0', '0'] + ['0', '1000', '0.707', '0', '1'] * 19
    params_disabled = build_params(input_eq_ch0=input_eq_ch0_disabled)
    responses_disabled = measure_at_frequencies('speakereq2x2', params_disabled, [100, 1000, 10000])
    
    if responses_disabled is None:
        result.fail_test("Failed to measure frequency response with enabled=0")
        return result
    
    # When enabled=1, 10kHz should be attenuated by the lowpass filter
    ch0_10k_enabled, ch1_10k_enabled = responses_enabled[10000]
    
    # When enabled=0, 10kHz should be flat (no attenuation)
    ch0_10k_disabled, ch1_10k_disabled = responses_disabled[10000]
    
    # Check that enabled=1 applies the filter (attenuation at 10kHz)
    if ch0_10k_enabled > -10:
        result.fail_test(f"Enabled=1: Expected significant attenuation at 10kHz, got {ch0_10k_enabled:.2f}dB")
        return result
    
    # Check that enabled=0 bypasses the filter (flat response at 10kHz)
    if abs(ch0_10k_disabled) > 0.5:
        result.fail_test(f"Enabled=0: Expected flat response at 10kHz, got {ch0_10k_disabled:.2f}dB")
        return result
    
    # Check that 100Hz is relatively flat in both cases (lowpass shouldn't affect it much)
    ch0_100_enabled, _ = responses_enabled[100]
    ch0_100_disabled, _ = responses_disabled[100]
    
    if abs(ch0_100_enabled) > 2.0:
        result.fail_test(f"Enabled=1: Expected flat response at 100Hz, got {ch0_100_enabled:.2f}dB")
        return result
    
    if abs(ch0_100_disabled) > 0.5:
        result.fail_test(f"Enabled=0: Expected flat response at 100Hz, got {ch0_100_disabled:.2f}dB")
        return result
    
    result.pass_test(f"Enabled=1: 10kHz={ch0_10k_enabled:.2f}dB (filtered), Enabled=0: 10kHz={ch0_10k_disabled:.2f}dB (bypassed)")
    return result

def main():
    """Run all tests and report results"""
    print("=" * 70)
    print("SpeakerEQ2x2 Plugin Unit Tests")
    print("=" * 70)
    print()
    
    # Clean up any existing config file to ensure tests use default parameters
    config_file = os.path.expanduser('~/.state/ladspa/speakereq2x2.ini')
    if os.path.exists(config_file):
        try:
            os.remove(config_file)
            print(f"Removed existing config file: {config_file}")
            print()
        except Exception as e:
            print(f"Warning: Could not remove config file: {e}")
            print()
    
    # Import save/load tests dynamically to avoid circular import
    import test_save_load
    
    tests = [
        test_passthrough,
        test_enable_disable_with_filter,
        test_lowpass_1khz,
        test_highpass_1khz,
        test_crossbar_swap,
        test_crossbar_mono_sum,
        test_crossbar_mute,
        test_bandpass_500_2k,
        test_peq_boost_3db,
        test_peq_cut_20db,
        test_filter_slots,
        test_notch_1khz,
        test_lowshelf_boost_6db,
        test_highshelf_cut_6db,
        test_hp1_1khz,
        test_lp1_1khz,
        test_all_filter_types,
        test_output_lowpass_1khz,
        test_output_peq_boost_3db,
        test_delay_with_zero_ms,
        test_delay_with_20ms,
        test_input_gain_6db,
        test_input_gain_minus_12db,
        test_output_gain_3db,
        test_combined_input_output_gain,
        test_independent_channel_gains,
        test_master_gain_10db,
        test_master_gain_minus_6db,
        test_master_gain_with_output_gain,
        test_save_load.test_save_load_master_gain_10db,
        test_save_load.test_save_load_master_gain_0db,
    ]
    
    results = []
    for test_func in tests:
        print(f"Running: {test_func.__name__}...", end=" ")
        sys.stdout.flush()
        result = test_func()
        results.append(result)
        
        if result.passed:
            print(f"✓ PASS")
            if result.message:
                print(f"  {result.message}")
        else:
            print(f"✗ FAIL")
            print(f"  {result.message}")
        print()
    
    # Summary
    print("=" * 70)
    passed = sum(1 for r in results if r.passed)
    total = len(results)
    print(f"Results: {passed}/{total} tests passed")
    
    if passed == total:
        print("All tests PASSED ✓")
        return 0
    else:
        print("Some tests FAILED ✗")
        return 1

if __name__ == '__main__':
    sys.exit(main())
