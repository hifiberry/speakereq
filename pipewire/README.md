# PipeWire Integration for SpeakerEQ2x2

This directory contains configuration files for integrating the SpeakerEQ2x2 LADSPA plugin with PipeWire.

## Quick Start

1. **Install the plugin:**
   ```bash
   cd /path/to/speakereq
   make
   sudo make install
   ```

2. **Install the configuration:**
   ```bash
   sudo make install-pipewire     # from the repository root
   ```
   or, equivalently:
   ```bash
   sudo ./pipewire/install.sh
   ```

3. **Restart PipeWire and WirePlumber:**
   ```bash
   systemctl --user restart pipewire wireplumber
   ```

## Configuration

The filter chain configuration (`90-speakereq.conf`, installed to
`/etc/pipewire/pipewire.conf.d/`) provides a complete example with:

- **Input EQ:** 20-band parametric EQ on each input channel (40 bands total)
- **Crossbar Mixer:** 2×2 routing matrix for flexible signal routing
- **Output EQ:** 20-band parametric EQ on each output channel (40 bands total)
- **Delay Compensation:** 0-100ms delay per output channel
- **Gain Controls:** Input and output gain adjustment per channel (-24 to +24 dB)

### Filter Types

Each EQ band supports these filter types (controlled by `type` parameter):

| Type | Name | Description |
|------|------|-------------|
| 0 | NOOP | No operation (bypass) |
| 1 | HP | High-pass (2nd order) |
| 2 | LP | Low-pass (2nd order) |
| 3 | PEQ | Parametric EQ (bell) |
| 4 | NOTCH | Notch filter |
| 5 | LS | Low shelf |
| 6 | HS | High shelf |
| 7 | HP1 | High-pass (1st order) |
| 8 | LP1 | Low-pass (1st order) |

### Parameter Ranges

- **Frequency (`f`):** 20 - 20000 Hz
- **Q Factor (`q`):** 0.1 - 10.0
- **Gain (`gain`):** -24 to +24 dB
- **Crossbar gains:** 0.0 - 2.0 (0 = mute, 1 = unity, 2 = +6dB)
- **Delay (`delay_X_ms`):** 0 - 100 ms

## Example Configurations

### Example 1: Room Correction

```conf
control = {
    # Tame room resonance at 80Hz
    "output_0_eq_1_type" = 3    # PEQ
    "output_0_eq_1_f"    = 80.0
    "output_0_eq_1_q"    = 2.0
    "output_0_eq_1_gain" = -6.0
    
    # Boost high frequencies lost to absorption
    "output_0_eq_2_type" = 6    # High shelf
    "output_0_eq_2_f"    = 8000.0
    "output_0_eq_2_q"    = 0.707
    "output_0_eq_2_gain" = 2.0
    
    # Apply same to right channel
    "output_1_eq_1_type" = 3
    "output_1_eq_1_f"    = 80.0
    "output_1_eq_1_q"    = 2.0
    "output_1_eq_1_gain" = -6.0
    
    "output_1_eq_2_type" = 6
    "output_1_eq_2_f"    = 8000.0
    "output_1_eq_2_q"    = 0.707
    "output_1_eq_2_gain" = 2.0
    
    # Crossbar: Identity
    "xbar_0_to_0" = 1.0
    "xbar_0_to_1" = 0.0
    "xbar_1_to_0" = 0.0
    "xbar_1_to_1" = 1.0
    
    # No delay
    "delay_0_ms" = 0.0
    "delay_1_ms" = 0.0
}
```

### Example 2: Time Alignment

```conf
control = {
    # No EQ, just crossbar and delay
    "xbar_0_to_0" = 1.0
    "xbar_0_to_1" = 0.0
    "xbar_1_to_0" = 0.0
    "xbar_1_to_1" = 1.0
    
    # Right speaker is 3.4 meters farther away
    # Speed of sound: 343 m/s
    # Delay needed: 3.4 / 343 = 9.9 ms
    "delay_0_ms" = 0.0
    "delay_1_ms" = 9.9
}
```

### Example 3: Subwoofer Crossover

```conf
control = {
    # Main speakers: High-pass at 80Hz
    "output_0_eq_1_type" = 1    # HP
    "output_0_eq_1_f"    = 80.0
    "output_0_eq_1_q"    = 0.707
    "output_0_eq_1_gain" = 0.0
    
    "output_1_eq_1_type" = 1
    "output_1_eq_1_f"    = 80.0
    "output_1_eq_1_q"    = 0.707
    "output_1_eq_1_gain" = 0.0
    
    # Crossbar: Identity
    "xbar_0_to_0" = 1.0
    "xbar_0_to_1" = 0.0
    "xbar_1_to_0" = 0.0
    "xbar_1_to_1" = 1.0
    
    "delay_0_ms" = 0.0
    "delay_1_ms" = 0.0
}
```

## Port Naming Convention

The plugin has 418 ports total:

- **Port 0:** `Enable` (0 = bypass, 1 = process)

- **Ports 1-4:** Audio I/O
  - `Input L`, `Input R`, `Output L`, `Output R`

- **Ports 5-204:** Input EQ (200 control ports)
  - 2 channels x 20 bands x 5 parameters
  - `input_<channel>_eq_<band>_type` (0-8)
  - `input_<channel>_eq_<band>_f` (20-20000 Hz)
  - `input_<channel>_eq_<band>_q` (0.1-10.0)
  - `input_<channel>_eq_<band>_gain` (-24 to +24 dB)
  - `input_<channel>_eq_<band>_enabled` (0 or 1)

- **Ports 205-208:** Crossbar (4 control ports)
  - `xbar_<input>_to_<output>` (0.0-2.0)

- **Ports 209-408:** Output EQ (200 control ports)
  - same five parameters per band as the input EQ

- **Ports 409-410:** Delay (2 control ports)
  - `delay_0_ms` (0-100 ms)
  - `delay_1_ms` (0-100 ms)

- **Ports 411-414:** Gain Controls (4 control ports)
  - `input_gain_0_db`, `input_gain_1_db`
  - `output_gain_0_db`, `output_gain_1_db`

- **Port 415:** `master_gain_db`
- **Port 416:** `save_settings` (rising edge persists the current settings)
- **Port 417:** `Licensed` (output, always 1)

## Verification

Check if the filter chain is loaded:

```bash
pw-cli list-objects | grep -A 5 "Speaker EQ"
```

Monitor CPU usage:

```bash
pw-top
```

Test the filter:

```bash
# Play a test tone
speaker-test -t sine -f 1000 -c 2
```

## Troubleshooting

### Plugin not found
- Verify installation: `ls -l /usr/local/lib/ladspa/speakereq2x2.so`
- Check plugin info: `analyseplugin /usr/local/lib/ladspa/speakereq2x2.so`

### No audio output
- Check PipeWire status: `systemctl --user status pipewire`
- View logs: `journalctl --user -u pipewire -f`
- Ensure filter chain is connected: `pw-link -o | grep speakereq`

### High CPU usage
- Reduce number of active filters (use NOOP for unused bands)
- Increase buffer size in PipeWire config
- Check for excessive delay values

### Configuration not loading
- Verify file location: `~/.config/pipewire/filter-chain.conf.d/speakereq2x2.conf`
- Check for syntax errors in the config file
- Restart PipeWire: `systemctl --user restart pipewire`

## Advanced Usage

### Runtime Parameter Control

All filter parameters can be changed on the fly without restarting PipeWire or interrupting audio playback using pw-cli.

#### View Current Parameters:

```bash
# Find the node ID
NODE_ID=$(pw-cli list-objects | grep -B 10 "Speaker EQ 2x2" | grep "id:" | head -1 | awk '{print $2}' | tr -d ',')

# View current parameters
pw-cli enum-params $NODE_ID Props
```

#### Change Parameters:

```bash
# Set a single parameter
pw-cli set-param $NODE_ID Props '{ "output_0_eq_1_gain": 3.0 }'

# Set multiple parameters at once
pw-cli set-param $NODE_ID Props '{
    "output_0_eq_1_type": 3,
    "output_0_eq_1_f": 1000.0,
    "output_0_eq_1_q": 2.0,
    "output_0_eq_1_gain": -3.0
}'
```

#### Parameter changes are immediate:
- No audio dropouts or interruptions
- Changes take effect within milliseconds
- Perfect for real-time tuning and room EQ adjustment
- Can be automated via scripts or external control software

#### Example: Real-time room correction
```bash
# Using pw-cli for fine-tuning
NODE_ID=$(pw-cli list-objects | grep -B 10 "Speaker EQ 2x2" | grep "id:" | head -1 | awk '{print $2}' | tr -d ',')

pw-cli set-param $NODE_ID Props '{ "output_0_eq_1_type": 3 }'     # PEQ
pw-cli set-param $NODE_ID Props '{ "output_0_eq_1_f": 85.0 }'     # 85 Hz
pw-cli set-param $NODE_ID Props '{ "output_0_eq_1_q": 3.0 }'      # Narrow
pw-cli set-param $NODE_ID Props '{ "output_0_eq_1_gain": -4.0 }'  # -4dB cut

# Fine-tune while listening
pw-cli set-param $NODE_ID Props '{ "output_0_eq_1_q": 2.5 }'      # Adjust Q
pw-cli set-param $NODE_ID Props '{ "output_0_eq_1_gain": -5.0 }'  # More cut
```

### Multiple Instances

You can run multiple instances of the plugin in series:

```conf
filter.graph = {
    nodes = [
        {
            name = speakereq_input
            # ... first instance config
        }
        {
            name = speakereq_output
            # ... second instance config
        }
    ]
    links = [
        { output = "speakereq_input:Out 0" input = "speakereq_output:In 0" }
        { output = "speakereq_input:Out 1" input = "speakereq_output:In 1" }
    ]
}

## See Also

- [PipeWire Documentation](https://docs.pipewire.org/)
- [PipeWire Filter Chain Module](https://gitlab.freedesktop.org/pipewire/pipewire/-/wikis/Filter-Chain)
- [SpeakerEQ2x2 README](../README.md)
