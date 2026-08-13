# SpeakerEQ2x2 Unit Tests

Automated unit tests for the speakereq2x2 LADSPA plugin.

## Running Tests

```bash
cd test
python3 test_speakereq2x2.py
```

## Test Coverage

The test suite validates the following functionality:

### 1. **Passthrough Test**
- Verifies that with all filters set to NOOP (type=0), audio passes through unmodified
- Checks response at low (100 Hz), mid (1 kHz), and high (10 kHz) frequencies
- Expects all frequencies within 0.5 dB of flat response

### 2. **1kHz Lowpass Filter**
- Tests lowpass filter on channel 0, band 1
- Validates -3 dB response at 1 kHz cutoff frequency
- Confirms attenuation >10 dB at 2 kHz
- Verifies flat response below 100 Hz
- Checks that channel 1 remains unaffected

### 3. **1kHz Highpass Filter**
- Tests highpass filter on channel 1, band 1
- Validates -3 dB response at 1 kHz cutoff frequency
- Confirms attenuation <-30 dB at 100 Hz
- Verifies flat response above 10 kHz
- Checks that channel 0 remains unaffected

### 4. **Crossbar Channel Swap**
- Channel 0: LP filter at 1 kHz
- Channel 1: HP filter at 1 kHz
- Crossbar: routes input 0 → output 1, input 1 → output 0
- Verifies that output 0 has HP characteristics (low at 100 Hz, high at 10 kHz)
- Verifies that output 1 has LP characteristics (high at 100 Hz, low at 10 kHz)

### 5. **Crossbar Mono Sum**
- Channel 0: LP filter at 1 kHz
- Channel 1: HP filter at 1 kHz
- Crossbar: sums both inputs to both outputs (all gains = 0.5)
- Verifies that both outputs are identical

### 6. **Crossbar Mute**
- All crossbar gains set to 0
- Verifies all outputs are muted (<-100 dB) at all frequencies

### 7. **Save/Load Parameters**
The `test_save_load.py` module tests parameter persistence functionality:

#### Test: Save/Load master_gain 10dB
1. Sets master_gain to 10dB and verifies gain is correct
2. Triggers save_settings parameter to save configuration
3. Verifies that configuration file (~/.state/ladspa/speakereq2x2.ini) is created
4. Confirms that master_gain_db is correctly saved as 10.0 in the config file

#### Test: Save/Load master_gain 0dB
1. Sets master_gain to 0dB (unity gain) and verifies gain is correct
2. Triggers save_settings parameter to save configuration
3. Verifies that configuration file is updated
4. Confirms that master_gain_db is correctly saved as 0.0 in the config file

These tests ensure that the plugin's save/load mechanism properly persists parameters to disk for restoration in future sessions.

## Test Implementation

Tests use the parent directory's `test-plugin.py` script to:
- Generate sine wave test signals
- Process through the LADSPA plugin with specified parameters
- Measure frequency response at multiple frequencies
- Validate that response matches expected behavior

## Expected Results

All tests should pass:
```
Results: X/X tests passed
All tests PASSED ✓
```

## Dependencies

- Python 3
- SoX (for audio processing)
- matplotlib (for frequency response plots, optional)
- speakereq2x2.so installed in /usr/local/lib/ladspa/
