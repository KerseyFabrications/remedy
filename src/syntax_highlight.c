/*
 * remedy - A full-featured markdown pager for modern Linux terminals
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "syntax_highlight.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *name;
    const char **keywords;
    const char **types;
    const char *line_comment;
    const char *block_comment_start;
    const char *block_comment_end;
    bool has_preproc;
    char string_chars[4];
} language_def_t;

/* clang-format off */
static const char *c_keywords[] = {
    "auto", "break", "case", "const", "continue", "default", "do", "else",
    "enum", "extern", "for", "goto", "if", "inline", "register", "restrict",
    "return", "sizeof", "static", "struct", "switch", "typedef", "union",
    "volatile", "while", NULL
};

static const char *c_types[] = {
    "bool", "char", "double", "float", "int", "long", "short", "signed",
    "unsigned", "void", "int8_t", "int16_t", "int32_t", "int64_t",
    "uint8_t", "uint16_t", "uint32_t", "uint64_t", "size_t", "ssize_t",
    "ptrdiff_t", "FILE", "NULL", "true", "false", NULL
};

static const char *py_keywords[] = {
    "and", "as", "assert", "async", "await", "break", "class", "continue",
    "def", "del", "elif", "else", "except", "finally", "for", "from",
    "global", "if", "import", "in", "is", "lambda", "nonlocal", "not",
    "or", "pass", "raise", "return", "try", "while", "with", "yield", NULL
};

static const char *py_types[] = {
    "True", "False", "None", "int", "float", "str", "list", "dict",
    "tuple", "set", "bool", "bytes", "type", "object", "self", NULL
};

static const char *js_keywords[] = {
    "async", "await", "break", "case", "catch", "class", "const",
    "continue", "debugger", "default", "delete", "do", "else", "export",
    "extends", "finally", "for", "function", "if", "import", "in",
    "instanceof", "let", "new", "of", "return", "static", "super",
    "switch", "this", "throw", "try", "typeof", "var", "void", "while",
    "with", "yield", NULL
};

static const char *js_types[] = {
    "true", "false", "null", "undefined", "NaN", "Infinity",
    "Array", "Object", "String", "Number", "Boolean", "Promise",
    "Map", "Set", "Symbol", NULL
};

static const char *rust_keywords[] = {
    "as", "async", "await", "break", "const", "continue", "crate", "dyn",
    "else", "enum", "extern", "fn", "for", "if", "impl", "in", "let",
    "loop", "match", "mod", "move", "mut", "pub", "ref", "return", "self",
    "static", "struct", "super", "trait", "type", "unsafe", "use", "where",
    "while", NULL
};

static const char *rust_types[] = {
    "bool", "char", "f32", "f64", "i8", "i16", "i32", "i64", "i128",
    "isize", "str", "u8", "u16", "u32", "u64", "u128", "usize",
    "String", "Vec", "Option", "Result", "Box", "Rc", "Arc",
    "Some", "None", "Ok", "Err", "true", "false", "Self", NULL
};

static const char *go_keywords[] = {
    "break", "case", "chan", "const", "continue", "default", "defer",
    "else", "fallthrough", "for", "func", "go", "goto", "if", "import",
    "interface", "map", "package", "range", "return", "select", "struct",
    "switch", "type", "var", NULL
};

static const char *go_types[] = {
    "bool", "byte", "complex64", "complex128", "error", "float32",
    "float64", "int", "int8", "int16", "int32", "int64", "rune",
    "string", "uint", "uint8", "uint16", "uint32", "uint64", "uintptr",
    "true", "false", "nil", "iota", "append", "cap", "close", "copy",
    "delete", "len", "make", "new", "panic", "print", "println",
    "recover", NULL
};

static const char *sh_keywords[] = {
    "if", "then", "else", "elif", "fi", "for", "while", "do", "done",
    "case", "esac", "in", "function", "select", "until", "return",
    "break", "continue", "local", "export", "readonly", "declare",
    "typeset", "source", "exit", "exec", "eval", "shift", "trap",
    "set", "unset", NULL
};

static const char *sh_types[] = {
    "true", "false", NULL
};

static const char *cpp_keywords[] = {
    "alignas", "alignof", "auto", "break", "case", "catch", "class",
    "const", "constexpr", "const_cast", "continue", "decltype", "default",
    "delete", "do", "dynamic_cast", "else", "enum", "explicit", "export",
    "extern", "for", "friend", "goto", "if", "inline", "mutable",
    "namespace", "new", "noexcept", "operator", "override", "private",
    "protected", "public", "register", "reinterpret_cast", "return",
    "sizeof", "static", "static_assert", "static_cast", "struct",
    "switch", "template", "this", "throw", "try", "typedef", "typeid",
    "typename", "union", "using", "virtual", "volatile", "while", NULL
};

static const char *java_keywords[] = {
    "abstract", "assert", "break", "case", "catch", "class", "const",
    "continue", "default", "do", "else", "enum", "extends", "final",
    "finally", "for", "goto", "if", "implements", "import", "instanceof",
    "interface", "native", "new", "package", "private", "protected",
    "public", "return", "static", "strictfp", "super", "switch",
    "synchronized", "this", "throw", "throws", "transient", "try",
    "volatile", "while", NULL
};

static const char *java_types[] = {
    "boolean", "byte", "char", "double", "float", "int", "long", "short",
    "void", "String", "Integer", "Boolean", "Object", "List", "Map",
    "Set", "ArrayList", "HashMap", "true", "false", "null", NULL
};
/* clang-format on */

static const language_def_t languages[] = {
    {"c", c_keywords, c_types, "//", "/*", "*/", true, "\"'"},
    {"h", c_keywords, c_types, "//", "/*", "*/", true, "\"'"},
    {"cpp", cpp_keywords, c_types, "//", "/*", "*/", true, "\"'"},
    {"c++", cpp_keywords, c_types, "//", "/*", "*/", true, "\"'"},
    {"cc", cpp_keywords, c_types, "//", "/*", "*/", true, "\"'"},
    {"cxx", cpp_keywords, c_types, "//", "/*", "*/", true, "\"'"},
    {"hpp", cpp_keywords, c_types, "//", "/*", "*/", true, "\"'"},
    {"java", java_keywords, java_types, "//", "/*", "*/", false, "\"'"},
    {"python", py_keywords, py_types, "#", NULL, NULL, false, "\"'"},
    {"py", py_keywords, py_types, "#", NULL, NULL, false, "\"'"},
    {"javascript", js_keywords, js_types, "//", "/*", "*/", false, "\"'`"},
    {"js", js_keywords, js_types, "//", "/*", "*/", false, "\"'`"},
    {"typescript", js_keywords, js_types, "//", "/*", "*/", false, "\"'`"},
    {"ts", js_keywords, js_types, "//", "/*", "*/", false, "\"'`"},
    {"rust", rust_keywords, rust_types, "//", "/*", "*/", false, "\"'"},
    {"rs", rust_keywords, rust_types, "//", "/*", "*/", false, "\"'"},
    {"go", go_keywords, go_types, "//", "/*", "*/", false, "\"'`"},
    {"golang", go_keywords, go_types, "//", "/*", "*/", false, "\"'`"},
    {"bash", sh_keywords, sh_types, "#", NULL, NULL, false, "\"'"},
    {"sh", sh_keywords, sh_types, "#", NULL, NULL, false, "\"'"},
    {"shell", sh_keywords, sh_types, "#", NULL, NULL, false, "\"'"},
    {"zsh", sh_keywords, sh_types, "#", NULL, NULL, false, "\"'"},
    {NULL, NULL, NULL, NULL, NULL, NULL, false, ""},
};

static const language_def_t *find_language(const char *name)
{
    if (!name || !name[0]) {
        return NULL;
    }

    for (int i = 0; languages[i].name; i++) {
        if (strcasecmp(languages[i].name, name) == 0) {
            return &languages[i];
        }
    }

    return NULL;
}

static bool is_word_char(char c)
{
    return isalnum((unsigned char) c) || c == '_';
}

static bool word_in_list(const char *word, size_t len, const char **list)
{
    if (!list) {
        return false;
    }

    for (int i = 0; list[i]; i++) {
        if (strlen(list[i]) == len && strncmp(list[i], word, len) == 0) {
            return true;
        }
    }

    return false;
}

typedef enum {
    TOKEN_NORMAL,
    TOKEN_KEYWORD,
    TOKEN_TYPE,
    TOKEN_STRING,
    TOKEN_COMMENT,
    TOKEN_NUMBER,
    TOKEN_PREPROC,
} token_type_t;

static color_id_t token_color(token_type_t type)
{
    switch (type) {
        case TOKEN_KEYWORD:
            return COLOR_SYN_KEYWORD;
        case TOKEN_TYPE:
            return COLOR_SYN_TYPE;
        case TOKEN_STRING:
            return COLOR_SYN_STRING;
        case TOKEN_COMMENT:
            return COLOR_SYN_COMMENT;
        case TOKEN_NUMBER:
            return COLOR_SYN_NUMBER;
        case TOKEN_PREPROC:
            return COLOR_SYN_PREPROC;
        default:
            return COLOR_CODE_BLOCK;
    }
}

static void emit_token(rendered_line_t *line, const char *text, size_t len, token_type_t type)
{
    if (len == 0) {
        return;
    }

    char *token_text = strndup(text, len);
    if (!token_text) {
        return;
    }

    style_flags_t style = STYLE_CODE;
    if (type == TOKEN_KEYWORD) {
        style |= STYLE_BOLD;
    }

    styled_span_t span = styled_span_create(token_text, style, token_color(type));
    rendered_line_append_span(line, &span);
    free(token_text);
}

static void
highlight_line(const char *line_text, size_t line_len, const language_def_t *lang, bool *in_block_comment, int indent, line_buffer_t *buf)
{
    rendered_line_t *rline = line_buffer_append_line(buf);
    if (!rline) {
        return;
    }
    rline->indent = indent;

    size_t i = 0;

    /* Continue block comment from previous line */
    if (*in_block_comment && lang->block_comment_end) {
        const char *end = strstr(line_text, lang->block_comment_end);
        if (end) {
            size_t comment_len = (size_t) (end - line_text) + strlen(lang->block_comment_end);
            emit_token(rline, line_text, comment_len, TOKEN_COMMENT);
            i                 = comment_len;
            *in_block_comment = false;
        } else {
            emit_token(rline, line_text, line_len, TOKEN_COMMENT);
            return;
        }
    }

    size_t normal_start = i;

    while (i < line_len) {
        /* Preprocessor directives */
        if (lang->has_preproc && line_text[i] == '#' && i == 0) {
            if (normal_start < i) {
                emit_token(rline, line_text + normal_start, i - normal_start, TOKEN_NORMAL);
            }
            emit_token(rline, line_text + i, line_len - i, TOKEN_PREPROC);
            return;
        }

        /* Line comments */
        if (lang->line_comment && strncmp(line_text + i, lang->line_comment, strlen(lang->line_comment)) == 0) {
            if (normal_start < i) {
                emit_token(rline, line_text + normal_start, i - normal_start, TOKEN_NORMAL);
            }
            emit_token(rline, line_text + i, line_len - i, TOKEN_COMMENT);
            return;
        }

        /* Block comments */
        if (lang->block_comment_start && strncmp(line_text + i, lang->block_comment_start, strlen(lang->block_comment_start)) == 0) {
            if (normal_start < i) {
                emit_token(rline, line_text + normal_start, i - normal_start, TOKEN_NORMAL);
            }

            const char *end = strstr(line_text + i + strlen(lang->block_comment_start), lang->block_comment_end);
            if (end) {
                size_t comment_len = (size_t) (end - (line_text + i)) + strlen(lang->block_comment_end);
                emit_token(rline, line_text + i, comment_len, TOKEN_COMMENT);
                i            = i + comment_len;
                normal_start = i;
                continue;
            } else {
                emit_token(rline, line_text + i, line_len - i, TOKEN_COMMENT);
                *in_block_comment = true;
                return;
            }
        }

        /* Strings */
        if (strchr(lang->string_chars, line_text[i])) {
            if (normal_start < i) {
                emit_token(rline, line_text + normal_start, i - normal_start, TOKEN_NORMAL);
            }

            char quote       = line_text[i];
            size_t str_start = i;
            i++;

            while (i < line_len) {
                if (line_text[i] == '\\' && i + 1 < line_len) {
                    i += 2;
                } else if (line_text[i] == quote) {
                    i++;
                    break;
                } else {
                    i++;
                }
            }

            emit_token(rline, line_text + str_start, i - str_start, TOKEN_STRING);
            normal_start = i;
            continue;
        }

        /* Numbers */
        if (isdigit((unsigned char) line_text[i]) && (i == 0 || !is_word_char(line_text[i - 1]))) {
            if (normal_start < i) {
                emit_token(rline, line_text + normal_start, i - normal_start, TOKEN_NORMAL);
            }

            size_t num_start = i;

            /* Hex prefix */
            if (line_text[i] == '0' && i + 1 < line_len && (line_text[i + 1] == 'x' || line_text[i + 1] == 'X')) {
                i += 2;
                while (i < line_len && isxdigit((unsigned char) line_text[i])) {
                    i++;
                }
            } else {
                while (i < line_len && (isdigit((unsigned char) line_text[i]) || line_text[i] == '.')) {
                    i++;
                }
            }

            /* Suffixes like UL, f, etc. */
            while (i < line_len && isalpha((unsigned char) line_text[i])) {
                i++;
            }

            emit_token(rline, line_text + num_start, i - num_start, TOKEN_NUMBER);
            normal_start = i;
            continue;
        }

        /* Words (keywords, types, or normal) */
        if (is_word_char(line_text[i]) && (i == 0 || !is_word_char(line_text[i - 1]))) {
            if (normal_start < i) {
                emit_token(rline, line_text + normal_start, i - normal_start, TOKEN_NORMAL);
            }

            size_t word_start = i;
            while (i < line_len && is_word_char(line_text[i])) {
                i++;
            }

            size_t word_len = i - word_start;

            if (word_in_list(line_text + word_start, word_len, lang->keywords)) {
                emit_token(rline, line_text + word_start, word_len, TOKEN_KEYWORD);
            } else if (word_in_list(line_text + word_start, word_len, lang->types)) {
                emit_token(rline, line_text + word_start, word_len, TOKEN_TYPE);
            } else {
                emit_token(rline, line_text + word_start, word_len, TOKEN_NORMAL);
            }

            normal_start = i;
            continue;
        }

        i++;
    }

    if (normal_start < line_len) {
        emit_token(rline, line_text + normal_start, line_len - normal_start, TOKEN_NORMAL);
    }
}

static int measure_line_display_width(const char *text, size_t len)
{
    int width = 0;
    for (size_t i = 0; i < len; i++) {
        if (text[i] == '\t') {
            width += 4 - (width % 4);
        } else {
            width++;
        }
    }
    return width;
}

static char *make_padding(int width, style_flags_t style, color_id_t color)
{
    if (width <= 0) {
        return NULL;
    }
    char *pad = malloc((size_t) width + 1);
    if (!pad) {
        return NULL;
    }
    memset(pad, ' ', (size_t) width);
    pad[width] = '\0';
    (void) style;
    (void) color;
    return pad;
}

static void pad_line_to_width(rendered_line_t *rline, int current_width, int block_width)
{
    int right_pad = block_width - current_width;
    if (right_pad <= 0) {
        return;
    }

    char *pad = make_padding(right_pad, STYLE_CODE, COLOR_CODE_BLOCK);
    if (pad) {
        styled_span_t span = styled_span_create(pad, STYLE_CODE, COLOR_CODE_BLOCK);
        rendered_line_append_span(rline, &span);
        free(pad);
    }
}

static bool contains_box_drawing(const char *text)
{
    if (!text) {
        return false;
    }
    /* Box-drawing characters are in Unicode block U+2500-U+257F, UTF-8: 0xE2 0x94 0x80-0xBF */
    for (const char *p = text; *p; p++) {
        if ((unsigned char) p[0] == 0xE2 && p[1] && (unsigned char) p[1] == 0x94) {
            return true;
        }
        /* Also check U+2580-U+259F block (block elements): 0xE2 0x96 */
        if ((unsigned char) p[0] == 0xE2 && p[1] && (unsigned char) p[1] == 0x96) {
            return true;
        }
    }
    return false;
}

static void render_diagram_block(const char *code, int indent, line_buffer_t *buf)
{
    const char *line_start = code;
    while (*line_start) {
        const char *line_end = strchr(line_start, '\n');
        if (!line_end) {
            line_end = line_start + strlen(line_start);
        }

        size_t line_len        = (size_t) (line_end - line_start);
        rendered_line_t *rline = line_buffer_append_line(buf);
        if (rline) {
            rline->indent = indent;
            if (line_len > 0) {
                char *text = strndup(line_start, line_len);
                if (text) {
                    styled_span_t span = styled_span_create(text, STYLE_CODE, COLOR_NORMAL);
                    rendered_line_append_span(rline, &span);
                    free(text);
                }
            }
        }

        line_start = (*line_end == '\n') ? line_end + 1 : line_end;
    }
}

int syntax_highlight_render(const char *code, const char *language, int indent, int available_width, line_buffer_t *buf)
{
    if (!code || !buf) {
        return FAILURE;
    }

    /* Detect diagram/ASCII art blocks — render verbatim without code block styling */
    if (contains_box_drawing(code)) {
        render_diagram_block(code, indent, buf);
        return SUCCESS;
    }

#define CODE_PAD_LEFT  2
#define CODE_PAD_RIGHT 2

    /* First pass: find max line width */
    int max_width          = 0;
    const char *line_start = code;
    while (*line_start) {
        const char *line_end = strchr(line_start, '\n');
        if (!line_end) {
            line_end = line_start + strlen(line_start);
        }
        size_t line_len = (size_t) (line_end - line_start);
        int width       = measure_line_display_width(line_start, line_len);
        if (width > max_width) {
            max_width = width;
        }
        line_start = (*line_end == '\n') ? line_end + 1 : line_end;
    }

    int block_width = max_width + CODE_PAD_LEFT + CODE_PAD_RIGHT;
    if (available_width > 0 && block_width > available_width - indent) {
        block_width = available_width - indent;
    }

    const language_def_t *lang = find_language(language);

    /* Top border: empty line at block width */
    rendered_line_t *top = line_buffer_append_line(buf);
    if (top) {
        top->indent = indent;
        char *pad   = make_padding(block_width, STYLE_CODE, COLOR_CODE_BLOCK);
        if (pad) {
            styled_span_t span = styled_span_create(pad, STYLE_CODE, COLOR_CODE_BLOCK);
            rendered_line_append_span(top, &span);
            free(pad);
        }
    }

    /* Second pass: render lines with padding */
    bool in_block_comment = false;
    line_start            = code;

    while (*line_start) {
        const char *line_end = strchr(line_start, '\n');
        if (!line_end) {
            line_end = line_start + strlen(line_start);
        }

        size_t line_len  = (size_t) (line_end - line_start);
        size_t start_idx = buf->line_count;

        if (lang) {
            highlight_line(line_start, line_len, lang, &in_block_comment, indent, buf);
        } else {
            rendered_line_t *rline = line_buffer_append_line(buf);
            if (rline) {
                rline->indent = indent;
                if (line_len > 0) {
                    char *text = strndup(line_start, line_len);
                    if (text) {
                        styled_span_t span = styled_span_create(text, STYLE_CODE, COLOR_CODE_BLOCK);
                        rendered_line_append_span(rline, &span);
                        free(text);
                    }
                }
            }
        }

        /* Add left padding and right padding to the line just added */
        if (buf->line_count > start_idx) {
            rendered_line_t *rline = &buf->lines[buf->line_count - 1];

            /* For empty lines, just pad the full block width */
            if (line_len == 0) {
                pad_line_to_width(rline, 0, block_width);
                line_start = (*line_end == '\n') ? line_end + 1 : line_end;
                continue;
            }

            /* Insert left padding at the beginning */
            char *left_pad = make_padding(CODE_PAD_LEFT, STYLE_CODE, COLOR_CODE_BLOCK);
            if (left_pad) {
                styled_span_t pad_span = styled_span_create(left_pad, STYLE_CODE, COLOR_CODE_BLOCK);
                free(left_pad);

                /* Shift existing spans right and insert padding at index 0 */
                rendered_line_append_span(rline, &pad_span);
                if (rline->span_count > 1) {
                    styled_span_t tmp = rline->spans[rline->span_count - 1];
                    memmove(&rline->spans[1], &rline->spans[0], (rline->span_count - 1) * sizeof(styled_span_t));
                    rline->spans[0] = tmp;
                }
            }

            /* Calculate current content width */
            int content_width = 0;
            for (size_t s = 0; s < rline->span_count; s++) {
                content_width += rline->spans[s].display_width;
            }

            /* Right-pad to block width */
            pad_line_to_width(rline, content_width, block_width);
        }

        line_start = (*line_end == '\n') ? line_end + 1 : line_end;
    }

    /* Bottom border: empty line at block width */
    rendered_line_t *bot = line_buffer_append_line(buf);
    if (bot) {
        bot->indent = indent;
        char *pad   = make_padding(block_width, STYLE_CODE, COLOR_CODE_BLOCK);
        if (pad) {
            styled_span_t span = styled_span_create(pad, STYLE_CODE, COLOR_CODE_BLOCK);
            rendered_line_append_span(bot, &span);
            free(pad);
        }
    }

    return SUCCESS;
}
