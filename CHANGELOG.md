# Changelog

All notable changes to remedy are documented here.

## 0.3.0 (2026-05-07)

### Added
- Document reload with `r` key — re-reads file from disk, preserving scroll position
- 1-character left/right margins for readability in split terminals
- Table cell word-wrap — long cell content wraps across multiple display lines
- Horizontal separator lines between table data rows
- Proportional column width distribution for tables that exceed terminal width

### Changed
- Version is now defined once in CMakeLists.txt — no more updating 5 files per release

## 0.2.1 (2026-05-05)

### Changed
- Code blocks render as uniform-width rectangles with left/right padding and consistent background fill

### Fixed
- Table columns with multi-byte Unicode (em dashes, CJK) now measure display width correctly instead of byte count

## 0.2.0 (2026-05-01)

### Added
- **Markdown diagnosis**: `remedy --diagnose file.md` or press `D` while viewing
  - Heading hierarchy (skipped levels, multiple H1s, missing H1)
  - Broken local image references
  - Missing image alt text
  - Empty or missing link URLs, links without display text
  - Code blocks missing language hints, empty code blocks
  - Table column count mismatches (raw-text pipe counting — catches issues cmark-gfm normalizes away)
  - Trailing whitespace, excessive blank lines
  - Exit code 0 = healthy, 1 = issues found (CI-friendly)
- Status bar shows health indicator (healthy / N issues)
- Diagnosis overlay with j/k navigation and Enter to jump to issue location
- Man page: `man remedy` works after install
- Multi-format inline images: JPEG, GIF, BMP via stb_image (previously PNG only)
- Kitty graphics transmit-once caching — image data sent once, placement is instant on scroll
- Images hidden during TOC/help/search overlays to prevent z-order issues
- Debian packaging (`packaging/build-deb.sh`), macOS build script, Homebrew formula template
- GitHub Actions CI: builds Linux .deb and macOS binary, creates releases on version tags

### Changed
- Word wrap completely rewritten: two-pass algorithm (O(n) instead of O(n^2) memmove), uses display columns via mbrtowc/wcwidth instead of byte counting
- Linked against ncursesw (wide character version) for proper UTF-8 rendering
- Blockquotes indented 4 columns (up from 2) for visual clarity
- Tables rendered with 2-column indent
- Unordered list bullets cycle by nesting depth: bullet, circle, square
- List bullet alignment matched between ordered and unordered
- Strikethrough uses real SGR 9 escape via post-draw overlay (not just A_DIM)
- Hyperlinks show URL in parentheses after link text, underlined with OSC 8
- Status bar: black on white for contrast, help hints right-aligned
- Help overlay includes all keybindings grouped by category

### Security
- Escape sequence injection: URLs and text sanitized before embedding in OSC 8 / SGR terminal escapes
- Image decompression bomb protection: 16M pixel limit after decode, STBI_MAX_DIMENSIONS=8192
- Path traversal protection: absolute paths and `..` rejected in image references
- Markdown input capped at 5MB (file and stdin)
- Base64 integer overflow check before length calculation
- Realloc failure in search highlight handled cleanly instead of writing past capacity

### Fixed
- Diagnose overlay "jump to line" actually navigates (was a no-op stub)
- Heading level bounds check (clamped to 1-6 with buffer headroom)
- IMAGE node children (alt text) no longer rendered as visible text after the image
- Paragraph enter no longer breaks list item lines (bullet/number stays on same line as text)
- Strip-continuations merges spans back into parent line (was destroying text, breaking re-wrap)
- Pager state zero-initialized (was using garbage for image tracking arrays)

### Removed
- Word wrap toggle (`w` key) — always wraps, simplifies UX
- `--nowrap` CLI flag
- Line length lint check — long paragraphs are normal in markdown

## 0.1.0 (2026-04-30)

### Added
- Initial release
- Full CommonMark + GFM markdown rendering: headings (6 levels, color-coded), bold, italic, inline code, fenced code blocks, blockquotes, ordered/unordered lists, links, images, horizontal rules, tables, strikethrough
- Syntax highlighting for C, C++, Python, JavaScript, TypeScript, Rust, Go, Java, Shell/Bash
- less-compatible keybindings: j/k, f/b, d/u, g/G, PgUp/PgDn, Home/End
- Forward/backward search with highlighting (/, ?, n, N)
- Table of contents overlay (t) with section jumping
- Help overlay (h)
- GFM tables with box-drawing borders and column alignment
- Inline images via Kitty graphics protocol (PNG)
- Terminal resize with automatic reflow
- Stdin pipe support (`cat file.md | remedy`)
- OSC 8 clickable hyperlinks
