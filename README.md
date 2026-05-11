# remedy

A full-featured `less`-like markdown pager for modern terminals.

remedy renders Markdown with full formatting — headings, bold, italic, code blocks with syntax highlighting, tables with box-drawing borders, clickable hyperlinks, inline images, and more — all inside a familiar pager interface.

## Features

- **Full CommonMark + GFM** rendering via cmark-gfm (tables, strikethrough, autolinks, tasklists)
- **Syntax highlighting** for C, C++, Python, JavaScript, TypeScript, Rust, Go, Java, Shell/Bash
- **`less`-compatible keybindings** — j/k, f/b, d/u, g/G, PgUp/PgDn, /, ?, n/N
- **Tables** with box-drawing borders, column alignment, and cell word-wrap
- **Inline images** via the Kitty graphics protocol (PNG, JPEG, GIF, BMP) with text fallback
- **Clickable hyperlinks** via OSC 8
- **Search** with highlighted matches (forward and backward)
- **Table of contents** overlay with section jumping (`t`)
- **Markdown diagnosis** — lint checks for heading hierarchy, broken images, missing alt text, empty links, and more (`D` or `--diagnose`)
- **Stdin pipe support** — `cat file.md | remedy`, `curl -s URL | remedy`
- **Live reload** — press `r` to re-read the file from disk
- **Terminal resize** with automatic reflow

## Install

### macOS (Homebrew)

```sh
brew tap KerseyFabrications/tap
brew install remedy
```

### Debian / Ubuntu (amd64, arm64)

Download the `.deb` from the [latest release](https://github.com/KerseyFabrications/remedy/releases/latest):

```sh
sudo dpkg -i remedy_*.deb
```

### From source

Works on any system with a C11 compiler, CMake, ncurses, and cmark-gfm.

```sh
# Debian/Ubuntu
sudo apt install cmake pkg-config gcc \
  libcmark-gfm-dev libcmark-gfm-extensions-dev libncurses-dev

# macOS
brew install cmake pkg-config cmark-gfm ncurses

# Build and install
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
make -C build -j$(nproc)
sudo cmake --install build
```

## Usage

```sh
remedy README.md                    # View a file
cat README.md | remedy              # Pipe from stdin
curl -sL URL | remedy               # Pipe from URL
remedy --diagnose README.md         # Lint check and exit
```

### Key bindings

| Key | Action |
|-----|--------|
| `q` | Quit |
| `j` / `k` / arrows | Scroll up/down |
| `f` / `b` / PgDn / PgUp | Page down/up |
| `d` / `u` | Half page down/up |
| `g` / `G` | Top / bottom |
| `/` / `?` | Search forward / backward |
| `n` / `N` | Next / previous match |
| `t` | Table of contents |
| `D` | Diagnose markdown issues |
| `r` | Reload file from disk |
| `h` | Help |

## Dependencies

| Library | Purpose |
|---------|---------|
| [cmark-gfm](https://github.com/github/cmark-gfm) | CommonMark parsing + GFM extensions |
| [ncursesw](https://invisible-island.net/ncurses/) | Terminal UI with wide-character/Unicode support |

Syntax highlighting is built-in with no external dependencies. Image support uses [stb_image](https://github.com/nothings/stb) (vendored).

## License

GPL-3.0-or-later
