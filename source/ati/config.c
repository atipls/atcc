#include "config.h"
#include "utils.h"
#include <ctype.h>

typedef enum {
    OPT_TOKEN_NONE,
    OPT_TOKEN_EOF,
    OPT_TOKEN_ERROR,
    OPT_TOKEN_STRING,
    OPT_TOKEN_IDENTIFIER,
    OPT_TOKEN_EQUALS,
    OPT_TOKEN_OPEN_BRACKET,
    OPT_TOKEN_CLOSE_BRACKET,
    OPT_TOKEN_AT,
    OPT_TOKEN_COMMA,
} OptTokenKind;

typedef struct {
    OptTokenKind kind;
    string value;
    u64 line;
    u64 column;
} OptToken;

typedef struct {
    string filename;
    string data;
    u64 index;
    u64 line;
    u64 column;
} OptLexer;

static void opt_lexer_init(OptLexer *lexer, string filename, string data) {
    *lexer = (OptLexer){
            .filename = filename,
            .data = data,
            .index = 0,
            .line = 1,
            .column = 1,
    };
}

static void opt_lexer_skip_whitespace(OptLexer *lexer) {
    while (lexer->index < lexer->data.length) {
        char c = lexer->data.data[lexer->index];
        if (isspace(c)) {
            if (c == '\n') {
                lexer->line++;
                lexer->column = 1;
            } else {
                lexer->column++;
            }
            lexer->index++;
        } else {
            break;
        }
    }
}

static OptToken opt_lexer_next(OptLexer *lexer) {
    OptToken token = (OptToken){.kind = OPT_TOKEN_NONE, .line = lexer->line, .column = lexer->column};

    if (lexer->index >= lexer->data.length) {
        token.kind = OPT_TOKEN_EOF;
        return token;
    }

    if (lexer->index >= lexer->data.length) {
        token.kind = OPT_TOKEN_EOF;
        return token;
    }

    opt_lexer_skip_whitespace(lexer);

    char c = lexer->data.data[lexer->index];

    if (c == '#') {
        while (lexer->index < lexer->data.length) {
            c = lexer->data.data[lexer->index];
            if (c == '\n') {
                lexer->line++;
                lexer->column = 1;

                opt_lexer_skip_whitespace(lexer);
                break;
            } else {
                lexer->column++;
            }
            lexer->index++;
        }

        opt_lexer_skip_whitespace(lexer);
    }

    c = lexer->data.data[lexer->index];

    if (c == '"') {
        lexer->index++;
        lexer->column++;

        u64 start = lexer->index;

        while (lexer->index < lexer->data.length) {
            c = lexer->data.data[lexer->index];

            if (c == '"') {
                lexer->index++;
                lexer->column++;

                token.kind = OPT_TOKEN_STRING;
                token.value = rawstr(lexer->data.data + start, lexer->index - start - 1);
                return token;
            }

            lexer->index++;
            lexer->column++;
        }

        token.kind = OPT_TOKEN_ERROR;
        return token;
    }

    if (isalpha(c) || c == '_' || c == '-') {
        u64 start = lexer->index;

        while (lexer->index < lexer->data.length) {
            c = lexer->data.data[lexer->index];

            if (!isalnum(c) && c != '_' && c != '-') {
                break;
            }

            lexer->index++;
            lexer->column++;
        }

        token.kind = OPT_TOKEN_IDENTIFIER;
        token.value = rawstr(lexer->data.data + start, lexer->index - start);
        return token;
    }

    if (c == '=') {
        lexer->index++;
        lexer->column++;

        token.kind = OPT_TOKEN_EQUALS;
        return token;
    }

    if (c == '[') {
        lexer->index++;
        lexer->column++;

        token.kind = OPT_TOKEN_OPEN_BRACKET;
        return token;
    }

    if (c == ']') {
        lexer->index++;
        lexer->column++;

        token.kind = OPT_TOKEN_CLOSE_BRACKET;
        return token;
    }

    if (c == '@') {
        lexer->index++;
        lexer->column++;

        token.kind = OPT_TOKEN_AT;
        return token;
    }

    if (c == ',') {
        lexer->index++;
        lexer->column++;

        token.kind = OPT_TOKEN_COMMA;
        return token;
    }

    if (lexer->index >= lexer->data.length) {
        token.kind = OPT_TOKEN_EOF;
        return token;
    }

    token.kind = OPT_TOKEN_ERROR;
    return token;
}


option *options_parse(string filename, Buffer buffer) {
    option *options = vector_create(option);

    string data = rawstr(buffer.data, buffer.length);
    OptLexer lexer;
    opt_lexer_init(&lexer, filename, data);

    option *current = null;

    OptToken token;
    while ((token = opt_lexer_next(&lexer)).kind != OPT_TOKEN_EOF) {
        if (token.kind == OPT_TOKEN_ERROR) {
            fprintf(stderr, "%s:%llu:%llu: error: unexpected token\n", filename.data, token.line, token.column);
            return null;
        }

        if (!current) {
            if (token.kind != OPT_TOKEN_IDENTIFIER) {
                fprintf(stderr, "%s:%llu:%llu: error: expected identifier\n", filename.data, token.line, token.column);
                return null;
            }

            current = vector_add(options, 1);
            current->key = token.value;
            current->entries = vector_create(entry);

            token = opt_lexer_next(&lexer);
            if (token.kind != OPT_TOKEN_EQUALS) {
                fprintf(stderr, "%s:%llu:%llu: error: expected '='\n", filename.data, token.line, token.column);
                return null;
            }

            continue;
        }


        if (token.kind == OPT_TOKEN_STRING) {
            entry *entry = vector_add(current->entries, 1);
            entry->value = token.value;

            current = null;
            continue;
        }

        if (token.kind == OPT_TOKEN_OPEN_BRACKET) {
            token = opt_lexer_next(&lexer);

            while (token.kind != OPT_TOKEN_CLOSE_BRACKET) {
                if (token.kind == OPT_TOKEN_ERROR) {
                    fprintf(stderr, "%s:%llu:%llu: error: unexpected token\n", filename.data, token.line, token.column);
                    return null;
                }

				string tag = str("");
                if (token.kind == OPT_TOKEN_AT) {
                    token = opt_lexer_next(&lexer);

                    if (token.kind != OPT_TOKEN_IDENTIFIER) {
                        fprintf(stderr, "%s:%llu:%llu: error: expected identifier\n", filename.data, token.line, token.column);
                        return null;
                    }

                    tag = token.value;

                    token = opt_lexer_next(&lexer);
                }

                if (token.kind != OPT_TOKEN_STRING) {
                    fprintf(stderr, "%s:%llu:%llu: error: expected string\n", filename.data, token.line, token.column);
                    return null;
                }

                entry *entry = vector_add(current->entries, 1);
                entry->value = token.value;
                entry->tag = tag;

                token = opt_lexer_next(&lexer);
                if (token.kind != OPT_TOKEN_COMMA)
                    break;
                token = opt_lexer_next(&lexer);
            }

            if (token.kind != OPT_TOKEN_CLOSE_BRACKET) {
                fprintf(stderr, "%s:%llu:%llu: error: expected ']'\n", filename.data, token.line, token.column);
                return null;
            }

            current = null;
        }
    }

    return options;
}

bool options_get(option *options, string key, string *value) {
    vector_foreach(option, option, options) {
        if (string_match(option->key, key)) {
            if (option->entries) {
                *value = option->entries->value;
                return true;
            }
        }
    }

    return false;
}

bool options_get_default(option *options, string key, string *value, string default_value) {
    if (options_get(options, key, value))
        return true;

    *value = default_value;
    return false;
}

bool options_get_list(option *options, string key, entry **value) {
    vector_foreach(option, option, options) {
        if (string_match(option->key, key)) {
            if (option->entries) {
                *value = option->entries;
                return true;
            }
        }
    }

    return false;
}
