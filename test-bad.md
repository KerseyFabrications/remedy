# Bad Markdown Example

This file is intentionally broken to test remedy's diagnosis feature.

# Oops, Another H1

Having two H1 headings is a structural problem.

#### Skipped Heading Level

We jumped from H1 straight to H4. Where did H2 and H3 go?

## Links Gone Wrong

Here's a link with [no URL]() and one with [](https://example.com) no display text.

## Images

![](missing-image.png)

![Good alt text](also-missing.jpg)

![Exists but no issues](wip.png)

## Code Blocks

```
No language hint on this code block.
```

```python
```

The block above is empty.

## Sloppy Formatting

This line has trailing whitespace.   
And this one too.	

Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur.




Too many blank lines above this paragraph.

## Table Issues

| Name | Status | Notes |
|------|--------|-------|
| Alpha | Done |
| Beta | Done | Works | Extra column here |

## The End

Press **D** to see the diagnosis, or run `remedy --diagnose test-bad.md` from the command line.
