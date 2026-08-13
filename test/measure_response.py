#!/usr/bin/env python3
"""
Precise frequency response measurement tool for LADSPA plugins.

Measures the frequency response at specific frequencies for testing.
Based on test-plugin.py but simplified for unit testing.
"""

import subprocess
import tempfile
import math
from pathlib import Path


def measure_at_frequencies(plugin_name, params, frequencies, sample_rate=48000):
    """
    Measure frequency response at specific frequencies.
    
    Args:
        plugin_name: Name of the LADSPA plugin (e.g., 'speakereq2x2')
        params: List of parameter values as strings
        frequencies: List of frequencies to measure (in Hz)
        sample_rate: Sample rate for processing (default: 48000)
    
    Returns:
        Dictionary mapping frequency to (ch0_db, ch1_db) tuples (relative gain in dB)
        Returns None if measurement fails
    """
    plugin_path = f"/usr/local/lib/ladspa/{plugin_name}.so"
    
    # Check if plugin exists
    if not Path(plugin_path).exists():
        print(f"Error: Plugin not found: {plugin_path}")
        return None
    
    # Get plugin info
    num_inputs, num_outputs = _get_plugin_info(plugin_path)
    if num_inputs is None:
        return None
    
    # First, measure reference (passthrough) at each frequency for normalization
    reference = {}
    for freq in frequencies:
        rms_values = _measure_frequency_direct(freq, num_inputs, sample_rate)
        if rms_values is not None:
            reference[freq] = rms_values
    
    results = {}
    
    for freq in frequencies:
        rms_values = _measure_frequency(
            plugin_path, num_inputs, num_outputs, 
            params, freq, sample_rate
        )
        
        if rms_values is not None and freq in reference:
            # Calculate relative gain in dB compared to reference
            ref_rms = reference[freq][0]  # Use first channel as reference
            db_values = []
            for rms in rms_values[:2]:  # First 2 output channels
                if ref_rms > 0 and rms > 0:
                    relative_db = 20 * math.log10(rms / ref_rms)
                    db_values.append(relative_db)
                else:
                    db_values.append(-200)  # Very low value for silence
            results[freq] = tuple(db_values)
        else:
            results[freq] = None
    
    return results


def _get_plugin_info(plugin_path):
    """Get number of inputs/outputs from plugin analysis."""
    try:
        result = subprocess.run(
            ['analyseplugin', plugin_path],
            capture_output=True,
            text=True
        )
        
        if result.returncode != 0:
            return None, None
        
        # Parse output for port counts
        num_inputs = 0
        num_outputs = 0
        
        for line in result.stdout.split('\n'):
            if 'input' in line.lower() and 'audio' in line.lower():
                num_inputs += 1
            elif 'output' in line.lower() and 'audio' in line.lower():
                num_outputs += 1
        
        return num_inputs, num_outputs
        
    except Exception as e:
        print(f"Error analyzing plugin: {e}")
        return None, None


def _measure_frequency_direct(frequency, num_channels, sample_rate):
    """
    Measure RMS amplitude of a direct (unprocessed) signal at a specific frequency.
    Used as reference for relative gain measurements.
    """
    # Calculate duration
    min_cycles = 100
    cycle_duration = 1.0 / frequency
    adaptive_duration = max(min_cycles * cycle_duration, 1.0)
    adaptive_duration = min(adaptive_duration, 10.0)
    
    # Trim parameters
    trim_start = 0.2 * adaptive_duration
    trim_len = 0.6 * adaptive_duration
    
    # Create temporary file
    with tempfile.NamedTemporaryFile(suffix='.wav', delete=False) as tmp:
        tmp_file = tmp.name
    
    try:
        # Generate signal
        sox_cmd = [
            'sox', '-n', '-r', str(sample_rate), tmp_file,
            'synth', str(adaptive_duration), 'sine', str(frequency),
            'gain', '-30',
            'channels', str(num_channels),
            'trim', str(trim_start), str(trim_len)
        ]
        
        result = subprocess.run(sox_cmd, capture_output=True, text=True)
        if result.returncode != 0:
            return None
        
        # Measure RMS for each channel
        rms_values = []
        for channel in range(num_channels):
            sox_stat_cmd = [
                'sox', tmp_file, '-n',
                'remix', str(channel + 1),
                'stat'
            ]
            
            result = subprocess.run(sox_stat_cmd, capture_output=True, text=True)
            if result.returncode != 0:
                rms_values.append(0.0)
                continue
            
            # Parse RMS
            rms = None
            for line in result.stderr.split('\n'):
                if 'RMS     amplitude:' in line or 'RMS lev dB' in line:
                    parts = line.split(':')
                    if len(parts) > 1:
                        try:
                            rms = float(parts[1].strip().split()[0])
                            break
                        except (ValueError, IndexError):
                            pass
            
            rms_values.append(rms if rms is not None else 0.0)
        
        return rms_values
        
    except Exception as e:
        return None
        
    finally:
        try:
            Path(tmp_file).unlink()
        except:
            pass


def _get_plugin_label(plugin_path):
    """Get the plugin label from analyseplugin output."""
    try:
        result = subprocess.run(
            ['analyseplugin', plugin_path],
            capture_output=True,
            text=True
        )
        
        if result.returncode != 0:
            return None
        
        # Parse output for label
        for line in result.stdout.split('\n'):
            if 'Label:' in line:
                parts = line.split(':', 1)
                if len(parts) > 1:
                    return parts[1].strip().strip('"')
        
        return None
        
    except Exception as e:
        print(f"Error getting plugin label: {e}")
        return None


def _measure_frequency(plugin_path, num_inputs, num_outputs, params, frequency, sample_rate):
    """
    Measure RMS amplitude at a specific frequency for each output channel.
    """
    # Calculate duration to capture at least 100 cycles, minimum 1 second
    min_cycles = 100
    cycle_duration = 1.0 / frequency
    adaptive_duration = max(min_cycles * cycle_duration, 1.0)
    adaptive_duration = min(adaptive_duration, 10.0)  # Cap at 10 seconds
    
    # Trim parameters
    trim_start = 0.2 * adaptive_duration
    trim_len = 0.6 * adaptive_duration
    
    # Create temporary files
    with tempfile.NamedTemporaryFile(suffix='.wav', delete=False) as tmp:
        tmp_input = tmp.name
    with tempfile.NamedTemporaryFile(suffix='.wav', delete=False) as tmp:
        tmp_output = tmp.name
    
    try:
        # Generate input signal with sox at -30dB to provide headroom for gain testing
        sox_cmd = [
            'sox', '-n', '-r', str(sample_rate), tmp_input,
            'synth', str(adaptive_duration), 'sine', str(frequency),
            'gain', '-30',  # Reduce by 30dB to allow headroom for boost
            'channels', str(num_inputs),
            'trim', str(trim_start), str(trim_len)
        ]
        
        result = subprocess.run(sox_cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"Error generating tone at {frequency} Hz: {result.stderr}")
            return None
        
        # Get plugin label from plugin path
        plugin_label = _get_plugin_label(plugin_path)
        if not plugin_label:
            print(f"Error: Could not find plugin label")
            return None
        
        # Process through LADSPA plugin using applyplugin
        apply_cmd = ['applyplugin', tmp_input, tmp_output, plugin_path, plugin_label] + params
        
        result = subprocess.run(apply_cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"Error applying plugin at {frequency} Hz: {result.stderr}")
            return None
        
        # Measure RMS for each output channel
        rms_values = []
        
        for channel in range(num_outputs):
            sox_stat_cmd = [
                'sox', tmp_output, '-n',
                'remix', str(channel + 1),
                'stat'
            ]
            
            result = subprocess.run(
                sox_stat_cmd,
                capture_output=True,
                text=True
            )
            
            if result.returncode != 0:
                rms_values.append(0.0)
                continue
            
            # Parse RMS from sox output
            rms = None
            for line in result.stderr.split('\n'):
                if 'RMS     amplitude:' in line or 'RMS lev dB' in line:
                    parts = line.split(':')
                    if len(parts) > 1:
                        try:
                            rms = float(parts[1].strip().split()[0])
                            break
                        except (ValueError, IndexError):
                            pass
            
            rms_values.append(rms if rms is not None else 0.0)
        
        return rms_values
        
    except Exception as e:
        print(f"Error measuring at {frequency} Hz: {e}")
        return None
        
    finally:
        # Clean up temporary files
        for f in [tmp_input, tmp_output]:
            try:
                Path(f).unlink()
            except:
                pass


def _rms_to_db(rms):
    """
    Convert RMS amplitude to dB.
    
    Applies correction for sine wave RMS = peak/sqrt(2)
    """
    if rms <= 0:
        return -200  # Very low value for silence
    
    # Convert to dB and correct for sine wave RMS offset
    # Sine wave RMS is peak/sqrt(2), so we add 20*log10(sqrt(2)) ≈ 3.0103 dB
    db = 20 * math.log10(rms) + 3.0103
    return db


if __name__ == '__main__':
    # Example usage
    import sys
    
    if len(sys.argv) < 3:
        print("Usage: measure_response.py <plugin_name> <freq1> <freq2> ... [params...]")
        sys.exit(1)
    
    plugin_name = sys.argv[1]
    
    # Parse frequencies and params
    frequencies = []
    params = []
    in_params = False
    
    for arg in sys.argv[2:]:
        if arg == '--':
            in_params = True
            continue
        
        if in_params:
            params.append(arg)
        else:
            try:
                frequencies.append(float(arg))
            except ValueError:
                print(f"Invalid frequency: {arg}")
                sys.exit(1)
    
    if not frequencies:
        print("No frequencies specified")
        sys.exit(1)
    
    # Measure
    results = measure_at_frequencies(plugin_name, params, frequencies)
    
    if results is None:
        print("Measurement failed")
        sys.exit(1)
    
    # Print results
    print(f"{'Frequency (Hz)':<15} {'Ch0 (dB)':<12} {'Ch1 (dB)':<12}")
    print("-" * 40)
    
    for freq in sorted(results.keys()):
        response = results[freq]
        if response is not None:
            ch0, ch1 = response
            print(f"{freq:<15.1f} {ch0:<12.2f} {ch1:<12.2f}")
        else:
            print(f"{freq:<15.1f} {'ERROR':<12} {'ERROR':<12}")
