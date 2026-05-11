# Remedy Test Document

This is a test markdown file for the **remedy** pager. It exercises every supported feature.

## Inline Formatting

- **Bold text** with double asterisks
- *Italic text* with single asterisks
- ***Bold and italic*** combined
- `Inline code` with backticks
- ~~Strikethrough~~ with double tildes
- A [hyperlink to GitHub](https://github.com) with OSC 8 support

## Ordered Lists

1. First ordered item
2. Second ordered item
3. Third ordered item

## Unordered Lists

- Unordered item one
- Unordered item two
- Unordered item three

## Nested Lists

- Top level item
    - Nested item one
    - Nested item two
        - Deeply nested
- Another top level item
    1. Nested ordered one
    2. Nested ordered two

1. A new top level ordered
    1. Nested ordered one
    2. Nested ordered two
2. Another top level
    1. Nested ordered one
    2. Nested ordered two

## Code Blocks with Syntax Highlighting

### C

```c
#include <stdio.h>
#include <stdlib.h>

/* A simple hello world program */
int main(int argc, char *argv[])
{
    const char *name = "remedy";
    int count = 42;
    printf("Hello from %s! Count: %d\n", name, count);
    return 0;
}
```

### Python

```python
import os
from pathlib import Path

class MarkdownReader:
    """A simple markdown reader."""

    def __init__(self, path: str):
        self.path = Path(path)
        self.lines = []

    def read(self) -> list[str]:
        # Read the file contents
        with open(self.path, "r") as f:
            self.lines = f.readlines()
        return self.lines

if __name__ == "__main__":
    reader = MarkdownReader("test.md")
    lines = reader.read()
    print(f"Read {len(lines)} lines")
```

### JavaScript

```javascript
const fs = require('fs');

async function readMarkdown(path) {
    const content = await fs.promises.readFile(path, 'utf-8');
    const lines = content.split('\n');
    console.log(`Read ${lines.length} lines`);
    return lines;
}

// Arrow function with template literal
const greet = (name) => `Hello, ${name}!`;
```

### Rust

```rust
use std::fs;
use std::io::Result;

fn read_markdown(path: &str) -> Result<Vec<String>> {
    let content = fs::read_to_string(path)?;
    let lines: Vec<String> = content.lines().map(|s| s.to_string()).collect();
    println!("Read {} lines", lines.len());
    Ok(lines)
}

fn main() {
    let result = read_markdown("test.md");
    match result {
        Ok(lines) => println!("Success: {} lines", lines.len()),
        Err(e) => eprintln!("Error: {}", e),
    }
}
```

### Shell

```bash
#!/bin/bash

# Read a markdown file and count lines
if [ -f "$1" ]; then
    lines=$(wc -l < "$1")
    echo "File has $lines lines"
else
    echo "File not found: $1"
    exit 1
fi

for i in 1 2 3; do
    echo "Processing pass $i..."
done
```

### Go

```go
package main

import (
    "fmt"
    "os"
    "strings"
)

func readMarkdown(path string) ([]string, error) {
    data, err := os.ReadFile(path)
    if err != nil {
        return nil, err
    }
    lines := strings.Split(string(data), "\n")
    fmt.Printf("Read %d lines\n", len(lines))
    return lines, nil
}

func main() {
    lines, err := readMarkdown("test.md")
    if err != nil {
        fmt.Fprintf(os.Stderr, "Error: %v\n", err)
        os.Exit(1)
    }
    fmt.Printf("Success: %d lines\n", len(lines))
}
```

### Unsupported Language (plain rendering)

```toml
[package]
name = "remedy"
version = "0.1.0"
edition = "2021"
```

## Blockquotes

> This is a blockquote.
> It can span multiple lines and contains **bold** and *italic* text.
>
> It can also have multiple paragraphs.

## Links

Check out [GitHub](https://github.com) for more info. Here is a [second link](https://example.com) to test multiple links on one line.

## Horizontal Rules

---

## Long Paragraph for Word Wrap Testing

Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.

## All Heading Levels

### Level 3 Heading

#### Level 4 Heading

##### Level 5 Heading

###### Level 6 Heading

## Tables (GFM)

| Feature | Status | Notes |
|---------|--------|-------|
| Headings | Done | Color-coded by level (6 colors) |
| Bold | Done | Uses A_BOLD attribute |
| Italic | Done | Uses A_ITALIC attribute |
| Inline Code | Done | Yellow with reverse video |
| Code Blocks | Done | Syntax highlighted |
| Blockquotes | Done | Vertical bar prefix |
| Lists | Done | Bullets, numbers, nesting |
| Links | Done | OSC 8 clickable hyperlinks |
| Strikethrough | Done | Dim text rendering |
| Tables | Done | Box-drawing borders |
| Search | Done | / and ? with highlighting |
| TOC | Done | Press t to open |
| Word Wrap | Done | Toggle with w |
| Resize | Done | Automatic reflow |

## Search Test

Try searching for the word **remedy** by pressing `/` and typing `remedy`. You should see all occurrences highlighted. Press `n` to jump to the next match and `N` for the previous.

## Keybindings Reference

| Key | Action |
|-----|--------|
| q | Quit |
| j / Down | Scroll down one line |
| k / Up | Scroll up one line |
| f / Space / PgDn | Page down |
| b / PgUp | Page up |
| d / Ctrl-D | Half page down |
| u / Ctrl-U | Half page up |
| g / Home | Top of document |
| G / End | Bottom of document |
| / | Search forward |
| ? | Search backward |
| n / N | Next / previous match |
| w | Toggle word wrap |
| t | Table of contents |
| Ctrl-L | Redraw screen |

## The End

That's all for now. Press **q** to quit, **t** for the table of contents, or **/** to search.
