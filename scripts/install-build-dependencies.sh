#!/bin/sh

set -e
CALLING_FROM="$PWD"
cd "$(dirname "$0")"

echo "Installing debian packages..."
sudo apt-get update -qq
sudo apt-get install -qq build-essential cmake git curl zip unzip tar

echo "Installing vcpkg..."
VCPKGTARGET="$HOME/.vcpkg"
if [ ! -d "$VCPKGTARGET" ]; then
    git clone https://github.com/microsoft/vcpkg.git "$VCPKGTARGET"
fi
cd "$VCPKGTARGET"
./bootstrap-vcpkg.sh -disableMetrics

INSTALLTARGET="$HOME/.local/bin"
mkdir -p "$INSTALLTARGET"
if [ ! -f "$INSTALLTARGET/vcpkg" ]; then
    ln -s "$VCPKGTARGET/vcpkg" "$INSTALLTARGET/vcpkg"
fi

PROFILEFILE="$HOME/.profile"
VCPKGEXPORT='export VCPKG_ROOT="$HOME/.local/opt/vcpkg"'
if [ -z "$(grep "$VCPKGEXPORT" $PROFILEFILE)" ]; then
    echo "Adding VCPKG_ROOT to $PROFILEFILE..."
    echo "$VCPKGEXPORT" >> "$PROFILEFILE"
    echo "WARNING: Log out and in again to reload your environment"
    echo "         vcpkg might not work as intended if you don't"
fi

echo "Done."
cd "$CALLING_FROM"
