#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
VERSION="0.1.0"

echo "=== Building remedy ${VERSION} for macOS ==="

# Check for Homebrew dependencies
if ! command -v brew &>/dev/null; then
    echo "Error: Homebrew is required. Install from https://brew.sh"
    exit 1
fi

for formula in cmake cmark-gfm; do
    if ! brew list "$formula" &>/dev/null 2>&1; then
        echo "Installing $formula..."
        brew install "$formula"
    fi
done

# ncurses ships with macOS, but Homebrew's version has better wide-char support
if ! brew list ncurses &>/dev/null 2>&1; then
    echo "Installing ncurses..."
    brew install ncurses
fi

# Set up pkg-config paths for Homebrew
BREW_PREFIX="$(brew --prefix)"
export PKG_CONFIG_PATH="${BREW_PREFIX}/opt/ncurses/lib/pkgconfig:${BREW_PREFIX}/opt/cmark-gfm/lib/pkgconfig:${PKG_CONFIG_PATH:-}"

# Build
echo "--- Building ---"
rm -rf "${PROJECT_DIR}/build"
mkdir -p "${PROJECT_DIR}/build"
cmake -S "$PROJECT_DIR" -B "${PROJECT_DIR}/build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local
make -C "${PROJECT_DIR}/build" -j"$(sysctl -n hw.logicalcpu)"

BINARY="${PROJECT_DIR}/build/remedy"
echo ""
echo "=== Build complete: ${BINARY} ==="
echo "Binary size: $(du -h "$BINARY" | cut -f1)"
echo ""
echo "To install system-wide:"
echo "  sudo cp ${BINARY} /usr/local/bin/remedy"
echo ""
echo "Or to create a Homebrew formula, see packaging/remedy.rb"
