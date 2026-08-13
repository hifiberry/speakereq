# SpeakerEQ2x2 LADSPA Plugin

Stereo speaker equalization plugin with input/output filterbanks and crossbar routing.

A complete 2-input, 2-output audio plugin with 20-band parametric EQ per channel and flexible routing.

## Features

- **Input Filterbanks**: 20-band parametric EQ per input channel (40 bands total)
- **Crossbar Mixer**: 2x2 routing matrix with adjustable gains
- **Output Filterbanks**: 20-band parametric EQ per output channel (40 bands total)
- **Output Delays**: Independent delay per output channel (0-100ms, default 0ms)
- **Gain Staging**: Per-channel input and output gain plus a master gain
- **9 Filter Types**: NOOP, HP, LP, PEQ, NOTCH, Low-Shelf, High-Shelf, HP1, LP1
- **Low Latency**: Hard real-time capable with direct-form II biquad filters
- **Multiple Sample Rates**: Works with any sample rate
- **Persistent Settings**: Settings can be saved to and reloaded from an INI file

## Signal Flow

```
Input L ──┬──> Input Filterbank L (20 bands) ──┐
          │                                    │
Input R ──┼──> Input Filterbank R (20 bands) ──┤
          │                                    │
          │         ┌─────────────┐            │
          └────────>│   Crossbar  │<───────────┘
                    │   2 × 2     │
                    │   Mixer     │
                    └─────┬───┬───┘
                          │   │
          ┌───────────────┘   └───────────────┐
          │                                   │
          ├──> Output Filterbank L (20 bands) ──> Delay L (0-100ms) ──> Output L
          │                                   │
          └──> Output Filterbank R (20 bands) ──> Delay R (0-100ms) ──> Output R
```

The full chain, including gain staging, is:

```
Input -> Input Gain -> Input Filterbank -> Crossbar -> Output Filterbank
      -> Delay -> Output Gain -> Master Volume -> Output
```

## Plugin Details

- **LADSPA Plugin ID**: 6840
- **Label**: `speakereq2x2`
- **Total Ports**: 418

### Port Structure

| Ports | Count | Purpose |
|-------|-------|---------|
| 0 | 1 | `Enable` (0 = bypass, 1 = process) |
| 1-4 | 4 | Audio: Input L/R, Output L/R |
| 5-204 | 200 | Input EQ: 2 channels x 20 bands x 5 parameters |
| 205-208 | 4 | Crossbar gains |
| 209-408 | 200 | Output EQ: 2 channels x 20 bands x 5 parameters |
| 409-410 | 2 | Output delays |
| 411-414 | 4 | Input gain L/R, output gain L/R |
| 415 | 1 | Master volume |
| 416 | 1 | Save settings |
| 417 | 1 | `Licensed` (output, always 1) |

Each EQ band exposes five parameters: `type`, `f` (frequency), `q`, `gain`, and
`enabled`.

The `Licensed` output port is retained so that existing control software keeps
working; it always reports 1.

### Filter Types

| Type | Name | Description | Parameters Used |
|------|------|-------------|-----------------|
| 0 | NOOP | Passthrough (no filtering) | - |
| 1 | HP | 2nd order highpass (Butterworth) | freq |
| 2 | LP | 2nd order lowpass (Butterworth) | freq |
| 3 | PEQ | Parametric EQ (peaking filter) | freq, q, gain |
| 4 | NOTCH | Notch filter (band reject) | freq, q |
| 5 | LS | Low-shelf filter | freq, q, gain |
| 6 | HS | High-shelf filter | freq, q, gain |
| 7 | HP1 | 1st order highpass | freq |
| 8 | LP1 | 1st order lowpass | freq |

### Parameter Ranges

- **Type**: 0-8 (integer)
- **Frequency**: 20-20000 Hz (logarithmic)
- **Q**: 0.1-10.0
- **Gain**: -24 to +24 dB
- **Crossbar Gains**: 0.0-2.0
- **Delay**: 0-100 ms

### Output Delays

Each output channel has an independent delay line:

- **Delay Range**: 0-100 milliseconds
- **Default**: 0 ms (no delay)
- **Buffer**: Dynamically allocated based on delay time and sample rate
- **Use Case**: Time alignment between speakers or compensation for acoustic delays

The delays are applied after the output filterbanks and before the final output.

### Persistent Settings

Settings are stored in `~/.state/ladspa/speakereq2x2.ini`. The file is read when
the plugin is instantiated, and written when the `save_settings` port is
triggered.

## Building

### Dependencies

```bash
sudo apt update
sudo apt install ladspa-sdk
```

`./install-dependencies.sh` installs the build and test dependencies for
Debian/Ubuntu, Fedora/RHEL, Arch, and openSUSE.

### Build

```bash
make all       # Build the plugin and regenerate architecture.png
make ladspa    # Build the plugin only
```

### Installation

```bash
sudo make install            # Plugin and man page
sudo make install-ladspa     # Plugin only
sudo make install-pipewire   # PipeWire and WirePlumber configuration
```

**Installation directory:** `/usr/local/lib/ladspa/speakereq2x2.so`

**Uninstall:**

```bash
sudo make uninstall
```

### Debian Package

```bash
make deb
```

This produces a `ladspa-speakereq` package that installs the plugin to
`/usr/lib/ladspa/`.

## Testing

The test suite drives the built plugin through SoX and measures the resulting
frequency response, so it needs `sox` and a built `speakereq2x2.so`.

```bash
make
cd test
python3 test_speakereq2x2.py
```

The test suite validates:

- All 9 filter types (HP, LP, PEQ, NOTCH, LS, HS, HP1, LP1)
- Input and output filterbanks
- Crossbar routing
- Gain staging and saved/restored settings
- Precise frequency response measurements at specified frequencies

### Manual Testing

**Analyze plugin information:**

```bash
analyseplugin /usr/local/lib/ladspa/speakereq2x2.so
```

## Usage

### With LADSPA Hosts

Load the plugin in any LADSPA-compatible host (e.g. Audacity, Ardour, ecasound).

```bash
# Example with ecasound
ecasound -i input.wav -o output.wav -el:speakereq2x2

# Example with sox
sox input.wav output.wav ladspa /usr/local/lib/ladspa/speakereq2x2.so speakereq2x2
```

### Filter Configuration Examples

**Simple lowpass on input left channel:**
- Input L Band 1: Type=LP (2), Freq=1000 Hz, Q=0.707

**Parametric boost on output:**
- Output L Band 1: Type=PEQ (3), Freq=3000 Hz, Q=2.0, Gain=+6 dB

**Crossbar routing (L+R to mono on both outputs):**
- Crossbar L->L: 0.5, L->R: 0.5, R->L: 0.5, R->R: 0.5

### PipeWire Integration

The `pipewire/` directory contains a ready-to-use filter chain configuration
and a WirePlumber rule that routes playback through the plugin. Install both
with:

```bash
sudo make install-pipewire
systemctl --user restart pipewire wireplumber
```

See [pipewire/README.md](pipewire/README.md) for the parameter reference,
example configurations, and runtime control via `pw-cli`.

## Development

### Code Structure

```
speakereq/
├── speakereq2x2_ladspa.c    # LADSPA plugin implementation
├── speakereq2x2_ladspa.h    # LADSPA plugin header
├── dsp/
│   ├── biquad.c/.h          # Biquad filter implementation
│   ├── filterbank.c/.h      # Cascaded biquad filterbanks
│   ├── crossbar.c/.h        # 2x2 routing matrix
│   └── delay.c/.h           # Output delay lines
├── utils/
│   ├── configfile.c/.h      # Settings persistence
│   └── ini.c/.h             # INI parser (inih, BSD-3-Clause)
├── pipewire/                # PipeWire and WirePlumber configuration
└── test/                    # Frequency response test suite
```

## License

MIT License - See [LICENSE](LICENSE) file for details.

The bundled INI parser in `utils/ini.c` and `utils/ini.h` is
[inih](https://github.com/benhoyt/inih) by Ben Hoyt, used under the
BSD-3-Clause license.

**Maker**: HiFiBerry
