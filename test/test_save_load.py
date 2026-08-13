#!/usr/bin/env python3
"""
Unit tests for speakereq2x2 save/load parameter functionality

Tests that parameters can be saved to config file and persist
across plugin instances without explicitly setting them.
"""

import sys
import os
import time

# Add parent directory to path
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# Import test utilities
test_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, test_dir)
from test_speakereq2x2 import TestResult, build_params
from measure_response import measure_at_frequencies


def test_save_load_master_gain_10db():
    """
    Test save/load with master_gain set to 10dB
    1. Set master_gain to 10dB and test gain
    2. Save parameters (trigger save_settings)
    3. Test again without explicitly setting master_gain - should still be 10dB
    """
    result = TestResult("Save/Load master_gain 10dB")
    
    # Clean up any existing config file first
    config_file = os.path.expanduser('~/.state/ladspa/speakereq2x2.ini')
    if os.path.exists(config_file):
        try:
            os.remove(config_file)
        except:
            pass
    
    # Step 1: Set master_gain to 10dB and measure
    params_10db = build_params(master_volume=10.0)
    
    # Measure response with 10dB master gain
    responses_10db = measure_at_frequencies('speakereq2x2', params_10db, [1000])
    
    if responses_10db is None:
        result.fail_test("Failed to measure frequency response with 10dB master gain")
        return result
    
    resp_1000_10db = responses_10db.get(1000)
    if not resp_1000_10db:
        result.fail_test("Missing 1kHz measurement with 10dB master gain")
        return result
    
    ch0_10db, ch1_10db = resp_1000_10db
    
    # Expected gain: 10dB (should be approximately +10dB)
    expected_10db = 10.0
    tolerance_db = 0.5
    
    if abs(ch0_10db - expected_10db) > tolerance_db or abs(ch1_10db - expected_10db) > tolerance_db:
        result.fail_test(f"Master gain 10dB incorrect: Ch0={ch0_10db:.2f}dB Ch1={ch1_10db:.2f}dB (expected {expected_10db:.2f}dB)")
        return result
    
    # Step 2: Save the parameters by triggering save_settings
    params_10db_save = build_params(master_volume=10.0, save_settings=1.0)
    
    responses_save = measure_at_frequencies('speakereq2x2', params_10db_save, [1000])
    
    if responses_save is None:
        result.fail_test("Failed to trigger save")
        return result
    
    # Give filesystem time to sync
    time.sleep(0.1)
    
    # Step 3: Verify config file has master_gain_db set to 10.0
    config_path = os.path.expanduser("~/.state/ladspa/speakereq2x2.ini")
    
    if not os.path.exists(config_path):
        result.fail_test(f"Config file not created at {config_path}")
        return result
    
    # Read config file and verify master_gain_db is set to 10.0
    with open(config_path, 'r') as f:
        config_content = f.read()
    
    if "master_gain_db" not in config_content:
        result.fail_test("master_gain_db not found in config file")
        return result
    
    # Parse the config to find master_gain_db value
    master_gain_found = False
    for line in config_content.split('\n'):
        if 'master_gain_db' in line and '=' in line:
            key, value = line.split('=', 1)
            key = key.strip()
            value = value.strip()
            if key == 'master_gain_db':
                master_gain_found = True
                try:
                    saved_gain = float(value)
                    if abs(saved_gain - 10.0) > 0.1:
                        result.fail_test(f"Saved master_gain_db incorrect: {saved_gain} (expected 10.0)")
                        return result
                except ValueError:
                    result.fail_test(f"Invalid master_gain_db value in config: {value}")
                    return result
                break
    
    if not master_gain_found:
        result.fail_test("master_gain_db key not found in config file")
        return result
    
    # Step 4: Test loading - run plugin again WITHOUT explicitly setting master_gain
    # and verify it loads from config file
    # Note: We pass 0.0 for master_volume but the plugin should load 10.0 from config
    params_load_test = build_params(master_volume=0.0, save_settings=0.0)
    
    responses_load = measure_at_frequencies('speakereq2x2', params_load_test, [1000])
    
    if responses_load is None:
        result.fail_test("Failed to measure frequency response after load")
        return result
    
    resp_1000_load = responses_load.get(1000)
    if not resp_1000_load:
        result.fail_test("Missing 1kHz measurement after load")
        return result
    
    ch0_load, ch1_load = resp_1000_load
    
    # If loading works, gain should still be 10dB (loaded from config)
    # If loading doesn't work, gain will be 0dB (from the parameter)
    if abs(ch0_load - expected_10db) > tolerance_db or abs(ch1_load - expected_10db) > tolerance_db:
        result.fail_test(f"Master gain NOT loaded from config. Expected 10dB but got Ch0={ch0_load:.2f}dB Ch1={ch1_load:.2f}dB. Config load may not be implemented.")
        return result
    
    result.pass_test(f"Master gain 10dB saved to {config_path} and successfully loaded (Ch0={ch0_load:.2f}dB, Ch1={ch1_load:.2f}dB)")
    return result


def test_save_load_master_gain_0db():
    """
    Test save/load with master_gain set to 0dB
    1. Set master_gain to 0dB and test gain
    2. Save parameters (trigger save_settings)
    3. Test again without explicitly setting master_gain - should still be 0dB
    """
    result = TestResult("Save/Load master_gain 0dB")
    
    # Clean up any existing config file first
    config_file = os.path.expanduser('~/.state/ladspa/speakereq2x2.ini')
    if os.path.exists(config_file):
        try:
            os.remove(config_file)
        except:
            pass
    
    # Step 1: Set master_gain to 0dB (unity gain) and measure
    params_0db = build_params(master_volume=0.0)
    
    # Measure response with 0dB master gain
    responses_0db = measure_at_frequencies('speakereq2x2', params_0db, [1000])
    
    if responses_0db is None:
        result.fail_test("Failed to measure frequency response with 0dB master gain")
        return result
    
    resp_1000_0db = responses_0db.get(1000)
    if not resp_1000_0db:
        result.fail_test("Missing 1kHz measurement with 0dB master gain")
        return result
    
    ch0_0db, ch1_0db = resp_1000_0db
    
    # Expected gain: 0dB (unity gain, approximately 0dB)
    expected_0db = 0.0
    tolerance_db = 0.5
    
    if abs(ch0_0db - expected_0db) > tolerance_db or abs(ch1_0db - expected_0db) > tolerance_db:
        result.fail_test(f"Master gain 0dB incorrect: Ch0={ch0_0db:.2f}dB Ch1={ch1_0db:.2f}dB (expected {expected_0db:.2f}dB)")
        return result
    
    # Step 2: Save the parameters by triggering save_settings
    params_0db_save = build_params(master_volume=0.0, save_settings=1.0)
    
    responses_save = measure_at_frequencies('speakereq2x2', params_0db_save, [1000])
    
    if responses_save is None:
        result.fail_test("Failed to trigger save")
        return result
    
    # Give filesystem time to sync
    time.sleep(0.1)
    
    # Step 3: Verify config file has master_gain_db set to 0.0
    config_path = os.path.expanduser("~/.state/ladspa/speakereq2x2.ini")
    
    if not os.path.exists(config_path):
        result.fail_test(f"Config file not found at {config_path}")
        return result
    
    # Read config file and verify master_gain_db is set to 0.0
    with open(config_path, 'r') as f:
        config_content = f.read()
    
    if "master_gain_db" not in config_content:
        result.fail_test("master_gain_db not found in config file")
        return result
    
    # Parse the config to find master_gain_db value
    master_gain_found = False
    for line in config_content.split('\n'):
        if 'master_gain_db' in line and '=' in line:
            key, value = line.split('=', 1)
            key = key.strip()
            value = value.strip()
            if key == 'master_gain_db':
                master_gain_found = True
                try:
                    saved_gain = float(value)
                    if abs(saved_gain - 0.0) > 0.1:
                        result.fail_test(f"Saved master_gain_db incorrect: {saved_gain} (expected 0.0)")
                        return result
                except ValueError:
                    result.fail_test(f"Invalid master_gain_db value in config: {value}")
                    return result
                break
    
    if not master_gain_found:
        result.fail_test("master_gain_db key not found in config file")
        return result
    
    # Step 4: Test loading - run plugin again WITHOUT explicitly setting master_gain
    # and verify it loads from config file
    # Note: We pass 0.0 for master_volume but the plugin should load 0.0 from config
    params_load_test = build_params(master_volume=10.0, save_settings=0.0)
    
    responses_load = measure_at_frequencies('speakereq2x2', params_load_test, [1000])
    
    if responses_load is None:
        result.fail_test("Failed to measure frequency response after load")
        return result
    
    resp_1000_load = responses_load.get(1000)
    if not resp_1000_load:
        result.fail_test("Missing 1kHz measurement after load")
        return result
    
    ch0_load, ch1_load = resp_1000_load
    
    # If loading works, gain should still be 0dB (loaded from config)
    # If loading doesn't work, gain will be 10dB (from the parameter)
    if abs(ch0_load - expected_0db) > tolerance_db or abs(ch1_load - expected_0db) > tolerance_db:
        result.fail_test(f"Master gain NOT loaded from config. Expected 0dB but got Ch0={ch0_load:.2f}dB Ch1={ch1_load:.2f}dB. Config load may not be implemented.")
        return result
    
    result.pass_test(f"Master gain 0dB saved to {config_path} and successfully loaded (Ch0={ch0_load:.2f}dB, Ch1={ch1_load:.2f}dB)")
    return result


def run_all_tests():
    """Run all save/load tests and report results"""
    tests = [
        test_save_load_master_gain_10db,
        test_save_load_master_gain_0db,
    ]
    
    print("=" * 70)
    print("Running Save/Load Parameter Tests")
    print("=" * 70)
    
    passed = 0
    failed = 0
    
    for test_func in tests:
        result = test_func()
        status = "PASS" if result.passed else "FAIL"
        symbol = "✓" if result.passed else "✗"
        
        print(f"\n{symbol} {result.name}: {status}")
        if result.message:
            print(f"  {result.message}")
        
        if result.passed:
            passed += 1
        else:
            failed += 1
    
    print("\n" + "=" * 70)
    print(f"Results: {passed} passed, {failed} failed, {passed + failed} total")
    print("=" * 70)
    
    return failed == 0


if __name__ == "__main__":
    success = run_all_tests()
    sys.exit(0 if success else 1)
