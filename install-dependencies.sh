#!/bin/bash
# Dependency installation script for the SpeakerEQ2x2 LADSPA plugin
# Installs build tools, LADSPA SDK, and testing dependencies

set -e  # Exit on error

echo "Installing dependencies for the SpeakerEQ2x2 LADSPA plugin..."
echo ""

# Detect Linux distribution
if [ -f /etc/os-release ]; then
    . /etc/os-release
    DISTRO=$ID
else
    echo "Cannot detect Linux distribution"
    exit 1
fi

echo "Detected distribution: $DISTRO"
echo ""

# Install dependencies based on distribution
case "$DISTRO" in
    ubuntu|debian|linuxmint|pop)
        echo "Installing packages for Debian/Ubuntu..."
        sudo apt-get update
        sudo apt-get install -y \
            build-essential \
            gcc \
            make \
            ladspa-sdk \
            sox \
            python3 \
            python3-matplotlib \
            graphviz \
            debhelper \
            devscripts
        ;;

    fedora|rhel|centos)
        echo "Installing packages for Fedora/RHEL..."
        sudo dnf install -y \
            gcc \
            make \
            ladspa-devel \
            sox \
            python3 \
            python3-matplotlib \
            graphviz \
            rpm-build \
            rpmdevtools
        ;;

    arch|manjaro)
        echo "Installing packages for Arch Linux..."
        sudo pacman -Sy --noconfirm \
            base-devel \
            ladspa \
            sox \
            python \
            python-matplotlib \
            graphviz
        ;;

    opensuse*)
        echo "Installing packages for openSUSE..."
        sudo zypper install -y \
            gcc \
            make \
            ladspa-devel \
            sox \
            python3 \
            python3-matplotlib \
            graphviz \
            rpm-build
        ;;

    *)
        echo "Unsupported distribution: $DISTRO"
        echo ""
        echo "Please install the following packages manually:"
        echo "  - Build tools (gcc, make)"
        echo "  - LADSPA SDK (development headers)"
        echo "  - SoX (Sound eXchange), used by the test suite"
        echo "  - Python 3"
        echo "  - Python matplotlib library (only for test-plugin.py plots)"
        echo "  - Graphviz (only for regenerating architecture.png)"
        echo "  - Package building tools (debhelper/rpm-build)"
        exit 1
        ;;
esac

echo ""
echo "Dependencies installed successfully!"
echo ""
echo "You can now:"
echo "  1. Build the plugin:         make all"
echo "  2. Install the plugin:       sudo make install"
echo "  3. Install PipeWire config:  sudo make install-pipewire"
echo "  4. Build Debian package:     make deb"
echo "  5. Run the test suite:       cd test && python3 test_speakereq2x2.py"
echo ""
