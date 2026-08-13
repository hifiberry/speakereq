#!/bin/bash
#
# Install SpeakerEQ2x2 PipeWire and WirePlumber configuration
#
# Usage: sudo ./install.sh
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PIPEWIRE_CONF_DIR="/etc/pipewire/pipewire.conf.d"
WIREPLUMBER_CONF_DIR="/etc/xdg/wireplumber/wireplumber.conf.d"

echo "Installing SpeakerEQ2x2 PipeWire configuration..."

# Check if plugin is installed
if [ ! -f "/usr/lib/ladspa/speakereq2x2.so" ] && \
   [ ! -f "/usr/local/lib/ladspa/speakereq2x2.so" ]; then
    echo "Error: Plugin not found in /usr/lib/ladspa or /usr/local/lib/ladspa"
    echo "Please build and install the plugin first:"
    echo "  cd .."
    echo "  make"
    echo "  sudo make install"
    exit 1
fi

mkdir -p "$PIPEWIRE_CONF_DIR" "$WIREPLUMBER_CONF_DIR"

cp "$SCRIPT_DIR/90-speakereq.conf" "$PIPEWIRE_CONF_DIR/90-speakereq.conf"
echo "Installed: $PIPEWIRE_CONF_DIR/90-speakereq.conf"

cp "$SCRIPT_DIR/99-force-effect-filter.conf" "$WIREPLUMBER_CONF_DIR/99-force-effect-filter.conf"
echo "Installed: $WIREPLUMBER_CONF_DIR/99-force-effect-filter.conf"

echo ""
echo "Installation complete!"
echo ""
echo "To activate the filter chain:"
echo "  systemctl --user restart pipewire wireplumber"
echo ""
echo "To verify the filter chain is loaded:"
echo "  pw-cli list-objects | grep -A 5 'Speaker EQ'"
echo ""
echo "To edit the configuration:"
echo "  \$EDITOR $PIPEWIRE_CONF_DIR/90-speakereq.conf"
echo ""
echo "To uninstall:"
echo "  rm $PIPEWIRE_CONF_DIR/90-speakereq.conf"
echo "  rm $WIREPLUMBER_CONF_DIR/99-force-effect-filter.conf"
echo "  systemctl --user restart pipewire wireplumber"
