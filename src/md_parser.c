/*
 * remedy - A full-featured markdown pager for modern Linux terminals
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "md_parser.h"

#include "remedy.h"

#include <cmark-gfm-core-extensions.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cmark_node *md_parser_parse(const char *text, size_t len)
{
    if (!text) {
        return NULL;
    }

    cmark_gfm_core_extensions_ensure_registered();

    cmark_parser *parser = cmark_parser_new(CMARK_OPT_DEFAULT);
    if (!parser) {
        return NULL;
    }

    cmark_syntax_extension *ext_table = cmark_find_syntax_extension("table");
    if (ext_table) {
        cmark_parser_attach_syntax_extension(parser, ext_table);
    }

    cmark_syntax_extension *ext_strike = cmark_find_syntax_extension("strikethrough");
    if (ext_strike) {
        cmark_parser_attach_syntax_extension(parser, ext_strike);
    }

    cmark_syntax_extension *ext_autolink = cmark_find_syntax_extension("autolink");
    if (ext_autolink) {
        cmark_parser_attach_syntax_extension(parser, ext_autolink);
    }

    cmark_syntax_extension *ext_tasklist = cmark_find_syntax_extension("tasklist");
    if (ext_tasklist) {
        cmark_parser_attach_syntax_extension(parser, ext_tasklist);
    }

    cmark_parser_feed(parser, text, len);
    cmark_node *doc = cmark_parser_finish(parser);
    cmark_parser_free(parser);

    return doc;
}

void md_parser_free(cmark_node *doc)
{
    if (doc) {
        cmark_node_free(doc);
    }
}

int md_parser_read_file(const char *path, char **out_text, size_t *out_len)
{
    if (!path || !out_text || !out_len) {
        return FAILURE;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
        return FAILURE;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size < 0) {
        fclose(fp);
        return FAILURE;
    }

    char *text = malloc((size_t) file_size + 1);
    if (!text) {
        fclose(fp);
        return FAILURE;
    }

    size_t bytes_read = fread(text, 1, (size_t) file_size, fp);
    fclose(fp);

    text[bytes_read] = '\0';
    *out_text        = text;
    *out_len         = bytes_read;

    return SUCCESS;
}

int md_parser_read_stdin(char **out_text, size_t *out_len)
{
    if (!out_text || !out_len) {
        return FAILURE;
    }

    size_t capacity = 4096;
    size_t length   = 0;
    char *text      = malloc(capacity);

    if (!text) {
        return FAILURE;
    }

    while (!feof(stdin)) {
        if (length + 4096 > capacity) {
            capacity *= 2;
            char *new_text = realloc(text, capacity);
            if (!new_text) {
                free(text);
                return FAILURE;
            }
            text = new_text;
        }

        size_t bytes_read = fread(text + length, 1, 4096, stdin);
        length += bytes_read;

        if (bytes_read == 0) {
            break;
        }
    }

    text[length] = '\0';
    *out_text    = text;
    *out_len     = length;

    return SUCCESS;
}
