#ifndef PORTABLE_PARSE_H
#define PORTABLE_PARSE_H
#include <stddef.h>
/* Shared bounded tokenizer for desktop and small MCU profiles. C99, no runtime.
 * Input must terminate within input_capacity. Exact in-place decoding is allowed;
 * other overlapping input/output regions are not. On error do not dispatch argv. */
typedef struct {
    unsigned argc;
    size_t error_at;
    const char *error;
} ps_parse_result;
static inline int ps_parse_tokens(const char *line, size_t input_capacity,
                                  char *storage, size_t capacity,
                                  char **argv, unsigned limit, ps_parse_result *result) {
    size_t i = 0, o = 0, length = 0;
    result->argc = 0;
    result->error_at = 0;
    result->error = 0;
    while (length < input_capacity && line[length]) ++length;
    if (length == input_capacity) { i = length; result->error = "line too long"; goto bad; }
    while (i < length) {
        unsigned quote = 0;
        while (i < length && (line[i] == ' ' || line[i] == '\t')) ++i;
        if (i == length || line[i] == '#') break;
        if (result->argc == limit) { result->error = "too many arguments"; goto bad; }
        argv[result->argc++] = storage + o;
        while (i < length) {
            unsigned char c = (unsigned char)line[i++];
            if (!quote && (c == ' ' || c == '\t')) break;
            if (c == '\'' || c == '"') {
                if (!quote) { quote = c; continue; }
                if (quote == c) { quote = 0; continue; }
            }
            if (c == '\\' && quote != '\'') {
                if (i == length) { result->error = "trailing escape"; goto bad; }
                c = (unsigned char)line[i++];
                if (c == 'n') c = '\n';
                else if (c == 't') c = '\t';
                else if (c == 'r') c = '\r';
            }
            if (o + 1 >= capacity) { result->error = "line too long"; goto bad; }
            storage[o++] = (char)c;
        }
        if (quote) { result->error = "unclosed quote"; goto bad; }
        if (o >= capacity) { result->error = "line too long"; goto bad; }
        storage[o++] = 0;
    }
    return 0;
bad:
    result->error_at = i;
    return -1;
}
#endif
