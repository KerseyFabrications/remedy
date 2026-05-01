/*
 * remedy - A full-featured markdown pager for modern Linux terminals
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MD_PARSER_H
#define MD_PARSER_H

#include <cmark-gfm.h>
#include <stddef.h>

/**
 * @brief Parse a markdown string into a cmark AST
 *
 * Initializes cmark-gfm with table, strikethrough, autolink, and tasklist
 * extensions, then parses the input text.
 *
 * @param text Markdown source text
 * @param len Length of text in bytes
 * @return Parsed AST root node, or NULL on failure. Caller must free with md_parser_free()
 */
cmark_node *md_parser_parse(const char *text, size_t len);

/**
 * @brief Free a parsed AST
 *
 * @param doc AST root node returned by md_parser_parse()
 */
void md_parser_free(cmark_node *doc);

/**
 * @brief Read an entire file into a heap-allocated buffer
 *
 * @param path File path to read
 * @param out_text Output pointer to allocated text (caller must free)
 * @param out_len Output length of text in bytes
 * @return SUCCESS or FAILURE
 */
int md_parser_read_file(const char *path, char **out_text, size_t *out_len);

/**
 * @brief Read all of stdin into a heap-allocated buffer
 *
 * @param out_text Output pointer to allocated text (caller must free)
 * @param out_len Output length of text in bytes
 * @return SUCCESS or FAILURE
 */
int md_parser_read_stdin(char **out_text, size_t *out_len);

#endif /* MD_PARSER_H */
