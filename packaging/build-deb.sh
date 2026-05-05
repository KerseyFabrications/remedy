#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
VERSION="0.2.1"
ARCH="$(dpkg --print-architecture)"
PKG_NAME="remedy"
PKG_DIR="${PROJECT_DIR}/packaging/${PKG_NAME}_${VERSION}_${ARCH}"

echo "=== Building remedy ${VERSION} for ${ARCH} ==="

# Check build dependencies
for dep in cmake pkg-config gcc; do
    if ! command -v "$dep" &>/dev/null; then
        echo "Error: $dep is required but not found"
        exit 1
    fi
done

for lib in libcmark-gfm-dev libncurses-dev; do
    if ! dpkg -s "$lib" &>/dev/null 2>&1; then
        echo "Error: $lib is required. Install with: sudo apt-get install $lib"
        exit 1
    fi
done

# Clean build
echo "--- Building ---"
rm -rf "${PROJECT_DIR}/build"
mkdir -p "${PROJECT_DIR}/build"
cmake -S "$PROJECT_DIR" -B "${PROJECT_DIR}/build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr
make -C "${PROJECT_DIR}/build" -j"$(nproc)"

# Create package directory structure
echo "--- Packaging ---"
rm -rf "$PKG_DIR"
mkdir -p "${PKG_DIR}/DEBIAN"
mkdir -p "${PKG_DIR}/usr/bin"

# Install binary
cp "${PROJECT_DIR}/build/remedy" "${PKG_DIR}/usr/bin/remedy"
strip "${PKG_DIR}/usr/bin/remedy"

# Install man page
mkdir -p "${PKG_DIR}/usr/share/man/man1"
cp "${PROJECT_DIR}/man/remedy.1" "${PKG_DIR}/usr/share/man/man1/remedy.1"
gzip -9 "${PKG_DIR}/usr/share/man/man1/remedy.1"

# Get dependency library versions
CMARK_VER=$(dpkg -s libcmark-gfm0.29.0.gfm.6 2>/dev/null | grep '^Version:' | cut -d' ' -f2 || echo "")
NCURSES_VER=$(dpkg -s libncursesw6 2>/dev/null | grep '^Version:' | cut -d' ' -f2 || echo "")

# Control file
cat > "${PKG_DIR}/DEBIAN/control" << EOF
Package: ${PKG_NAME}
Version: ${VERSION}
Section: text
Priority: optional
Architecture: ${ARCH}
Depends: libcmark-gfm0.29.0.gfm.6, libcmark-gfm-extensions0.29.0.gfm.6, libncursesw6
Maintainer: Kris Kersey <kris.kersey@flocksafety.com>
Homepage: https://github.com/kkersey/remedy
Description: Full-featured markdown pager for modern Linux terminals
 remedy is a less-like pager that renders Markdown with full formatting
 support including syntax-highlighted code blocks, tables with box-drawing
 borders, clickable hyperlinks, inline images (Kitty graphics protocol),
 search with highlighting, and a table of contents overlay.
EOF

# Build the .deb
DEB_FILE="${PROJECT_DIR}/packaging/${PKG_NAME}_${VERSION}_${ARCH}.deb"
dpkg-deb --build "$PKG_DIR" "$DEB_FILE"
rm -rf "$PKG_DIR"

echo ""
echo "=== Package built: ${DEB_FILE} ==="
echo "Install with: sudo dpkg -i ${DEB_FILE}"
echo "Size: $(du -h "$DEB_FILE" | cut -f1)"
