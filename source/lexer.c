#include "atcc.h"
#include "ati/utils.h"
#include <ctype.h>

typedef struct Lexer {
    string filename;
    Buffer source;
    u64 offset;

    i32 line;
    i32 column;

    Token *buffer;
} Lexer;

static i8 lexer_read(Lexer *lexer) {
    if (lexer->offset >= lexer->source.length)
        return 0;
    i8 character = lexer->source.data[lexer->offset++];
    if (character == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    return character;
}

static i8 lexer_peek(Lexer *lexer) {
    if (lexer->offset >= lexer->source.length)
        return 0;
    return lexer->source.data[lexer->offset];
}

static i8 lexer_peek_next(Lexer *lexer) {
    if (lexer->offset + 1 >= lexer->source.length)
        return 0;
    return lexer->source.data[lexer->offset + 1];
}

static void lexer_build_identifier(Lexer *lexer, Token *token) {
#define initstr(s) \
    { .data = (i8 *) (s), .length = sizeof(s) - 1 }
    static struct {
        string ident;
        TokenKind kind;
    } keywords[] = {
            {initstr("alias"), TOKEN_KW_ALIAS},
            {initstr("alignof"), TOKEN_KW_ALIGNOF},
            {initstr("break"), TOKEN_KW_BREAK},
            {initstr("case"), TOKEN_KW_CASE},
            {initstr("cast"), TOKEN_KW_CAST},
            {initstr("const"), TOKEN_KW_CONST},
            {initstr("continue"), TOKEN_KW_CONTINUE},
            {initstr("default"), TOKEN_KW_DEFAULT},
            {initstr("do"), TOKEN_KW_DO},
            {initstr("else"), TOKEN_KW_ELSE},
            {initstr("enum"), TOKEN_KW_ENUM},
            {initstr("for"), TOKEN_KW_FOR},
            {initstr("fun"), TOKEN_KW_FUN},
            {initstr("if"), TOKEN_KW_IF},
            {initstr("offsetof"), TOKEN_KW_OFFSETOF},
            {initstr("return"), TOKEN_KW_RETURN},
            {initstr("sizeof"), TOKEN_KW_SIZEOF},
            {initstr("struct"), TOKEN_KW_STRUCT},
            {initstr("switch"), TOKEN_KW_SWITCH},
            {initstr("union"), TOKEN_KW_UNION},
            {initstr("var"), TOKEN_KW_VAR},
            {initstr("while"), TOKEN_KW_WHILE},
    };
#undef initstr

    i8 *buffer = vector_create_n(i8, 32);
    while (isalnum(lexer_peek(lexer)) || lexer_peek(lexer) == '_')
        vector_push(buffer, lexer_read(lexer));

    token->kind = TOKEN_IDENTIFIER;
    token->value = rawstr(buffer, vector_length(buffer));
    for (i32 i = 0; i < array_length(keywords); i++) {
        if (string_match(token->value, keywords[i].ident)) {
            token->kind = keywords[i].kind;
            return;
        }
    }
}

static void lexer_build_number(Lexer *lexer, Token *token) {
    i8 *buffer = vector_create_n(i8, 32);
    bool seen_dot = 0;
    while (isdigit(lexer_peek(lexer)) || lexer_peek(lexer) == '.') {
        if (lexer_peek(lexer) == '.') {
            if (!seen_dot)
                seen_dot = true;
            else {
                token->kind = TOKEN_NUMBER;
                token->value = rawstr(buffer, vector_length(buffer));
                lexer->offset--;// Make sure .. lexes properly.
                return;
            }
        }
        vector_push(buffer, lexer_read(lexer));
    }

    token->kind = TOKEN_NUMBER;
    token->value = rawstr(buffer, vector_length(buffer));
}

static void lexer_build_string_or_char(Lexer *lexer, Token *token, i8 terminator) {
    i8 *buffer = vector_create_n(i8, 32);
    while (lexer_peek(lexer) != terminator) {
        i8 character = lexer_read(lexer);
        if (character == '\\') {
            i8 maybe_escape = lexer_read(lexer);
            switch (maybe_escape) {
                case 'n': vector_push(buffer, '\n'); break;
                case 'r': vector_push(buffer, '\r'); break;
                case 't': vector_push(buffer, '\t'); break;
                case '0': vector_push(buffer, '\0'); break;
                case '\\': vector_push(buffer, '\\'); break;
                case '\'': vector_push(buffer, '\''); break;
                case '"': vector_push(buffer, '"'); break;
                case 'x':
                case 'X': {
                    u8 hex_digit = 0;
                    while (isxdigit(lexer_peek(lexer))) {
                        u8 hex_character = lexer_read(lexer);
                        u8 number = isdigit(hex_character) ? hex_character - '0' : hex_character - 'A' + 10;
                        hex_digit = hex_digit * 16 + number;
                    }
                    vector_push(buffer, hex_digit);
                    break;
                }
                default:
                    vector_push(buffer, character);
                    vector_push(buffer, maybe_escape);
                    break;
            }
        } else {
            vector_push(buffer, character);
        }
    }

    lexer_read(lexer);
    vector_push(buffer, '\0');

    token->kind = terminator == '"' ? TOKEN_STRING : TOKEN_CHAR;
    token->value = rawstr(buffer, vector_length(buffer));
}

static void lexer_build_equal(Lexer *lexer, Token *token, TokenKind token1, TokenKind token2) {
    token->kind = token1;
    if (lexer_peek(lexer) == '=') {
        lexer_read(lexer);
        token->kind = token2;
    }
}

static void lexer_skip_multiline_comment(Lexer *lexer) {
    int level = 1;
    while (lexer_peek(lexer) != 0 && level > 0) {
        if (lexer_peek(lexer) == '/' && lexer_peek_next(lexer) == '*') {
            level++;
            lexer_read(lexer);
            lexer_read(lexer);
        } else if (lexer_peek(lexer) == '*' && lexer_peek_next(lexer) == '/') {
            level--;
            lexer_read(lexer);
            lexer_read(lexer);
        } else {
            lexer_read(lexer);
        }
    }
}

Token *lexer_tokenize(string filename, Buffer data) {
    Lexer lexer = {0};
    lexer.filename = filename;
    lexer.source = data;
    lexer.line = 1;
    lexer.column = 1;
    lexer.buffer = vector_create(Token);

    while (lexer_peek(&lexer) != 0) {
        while (isspace(lexer_peek(&lexer)) || lexer_peek(&lexer) == -62 || lexer_peek(&lexer) == -96)
            lexer_read(&lexer);

        vector_push(lexer.buffer, (Token){0});
        Token *current = &vector_last(lexer.buffer);

        current->kind = TOKEN_NONE;
        current->flags = TOKEN_FLAG_NONE;
        current->location = (Location){.file = lexer.filename, .line = lexer.line, .column = lexer.column};
        current->value = str("");

        if (lexer_peek(&lexer) == 0)
            current->kind = TOKEN_EOF;

        if (isalpha(lexer_peek(&lexer)) || lexer_peek(&lexer) == '_') {
            lexer_build_identifier(&lexer, current);
            continue;
        }

        if (isdigit(lexer_peek(&lexer))) {
            lexer_build_number(&lexer, current);
            continue;
        }

        if (lexer_peek(&lexer) == '"' || lexer_peek(&lexer) == '\'') {
            lexer_build_string_or_char(&lexer, current, lexer_read(&lexer));
            continue;
        }

        i8 character = lexer_read(&lexer);
        switch (character) {
            case '\0': current->kind = TOKEN_EOF; break;
            case '+': lexer_build_equal(&lexer, current, TOKEN_PLUS, TOKEN_PLUS_EQUAL); break;
            case '-': lexer_build_equal(&lexer, current, TOKEN_MINUS, TOKEN_MINUS_EQUAL); break;
            case '*': lexer_build_equal(&lexer, current, TOKEN_STAR, TOKEN_STAR_EQUAL); break;
            case '%': lexer_build_equal(&lexer, current, TOKEN_PERCENT, TOKEN_PERCENT_EQUAL); break;
            case '^': lexer_build_equal(&lexer, current, TOKEN_CARET, TOKEN_CARET_EQUAL); break;
            case '~': lexer_build_equal(&lexer, current, TOKEN_TILDE, TOKEN_TILDE_EQUAL); break;
            case '=': lexer_build_equal(&lexer, current, TOKEN_EQUAL, TOKEN_EQUAL_EQUAL); break;
            case '!': lexer_build_equal(&lexer, current, TOKEN_EXCLAMATION, TOKEN_EXCLAMATION_EQUAL); break;
            case ':': lexer_build_equal(&lexer, current, TOKEN_COLON, TOKEN_COLON_EQUAL); break;
            case '.':
                current->kind = TOKEN_DOT;
                if (lexer_peek(&lexer) == '.') {
                    lexer_read(&lexer);
                    current->kind = TOKEN_DOT_DOT;
                }
                break;
            case ',': current->kind = TOKEN_COMMA; break;
            case '?': current->kind = TOKEN_QUESTION; break;
            case ';': current->kind = TOKEN_SEMICOLON; break;
            case '(': current->kind = TOKEN_OPEN_PAREN; break;
            case ')': current->kind = TOKEN_CLOSE_PAREN; break;
            case '[': current->kind = TOKEN_OPEN_BRACKET; break;
            case ']': current->kind = TOKEN_CLOSE_BRACKET; break;
            case '{': current->kind = TOKEN_OPEN_BRACE; break;
            case '}': current->kind = TOKEN_CLOSE_BRACE; break;
            case '#': current->kind = TOKEN_HASH; break;
            case '$': current->kind = TOKEN_DOLLAR; break;
            case '@': current->kind = TOKEN_AT; break;
            case '&': {
                current->kind = TOKEN_AMPERSAND;
                if (lexer_peek(&lexer) == '&') {
                    lexer_read(&lexer);
                    current->kind = TOKEN_AMPERSAND_AMPERSAND;
                    if (lexer_peek(&lexer) == '=') {
                        lexer_read(&lexer);
                        current->kind = TOKEN_AMPERSAND_AMPERSAND_EQUAL;
                    }
                } else if (lexer_peek(&lexer) == '=') {
                    lexer_read(&lexer);
                    current->kind = TOKEN_AMPERSAND_EQUAL;
                }
                break;
            }
            case '|': {
                current->kind = TOKEN_PIPE;
                if (lexer_peek(&lexer) == '|') {
                    lexer_read(&lexer);
                    current->kind = TOKEN_PIPE_PIPE;
                    if (lexer_peek(&lexer) == '=') {
                        lexer_read(&lexer);
                        current->kind = TOKEN_PIPE_PIPE_EQUAL;
                    }
                } else if (lexer_peek(&lexer) == '=') {
                    lexer_read(&lexer);
                    current->kind = TOKEN_PIPE_EQUAL;
                }
                break;
            }
            case '<': {
                current->kind = TOKEN_LESS;
                if (lexer_peek(&lexer) == '<') {
                    lexer_read(&lexer);
                    current->kind = TOKEN_LEFT_SHIFT;
                    if (lexer_peek(&lexer) == '=') {
                        lexer_read(&lexer);
                        current->kind = TOKEN_LEFT_SHIFT_EQUAL;
                    }
                } else if (lexer_peek(&lexer) == '=') {
                    lexer_read(&lexer);
                    current->kind = TOKEN_LESS_EQUAL;
                }
                break;
            }
            case '>': {
                current->kind = TOKEN_GREATER;
                if (lexer_peek(&lexer) == '>') {
                    lexer_read(&lexer);
                    current->kind = TOKEN_RIGHT_SHIFT;
                    if (lexer_peek(&lexer) == '=') {
                        lexer_read(&lexer);
                        current->kind = TOKEN_RIGHT_SHIFT_EQUAL;
                    }
                } else if (lexer_peek(&lexer) == '=') {
                    lexer_read(&lexer);
                    current->kind = TOKEN_GREATER_EQUAL;
                }
                break;
            }
            case '/': {
                current->kind = TOKEN_SLASH;
                if (lexer_peek(&lexer) == '=') {
                    lexer_read(&lexer);
                    current->kind = TOKEN_SLASH_EQUAL;
                } else if (lexer_peek(&lexer) == '/') {// Single line comment
                    lexer_read(&lexer);
                    while (lexer_peek(&lexer) != '\0' && lexer_peek(&lexer) != '\r' && lexer_peek(&lexer) != '\n')
                        lexer_read(&lexer);
                    vector_header(lexer.buffer)->length--;// We don't want to emit the slash if we are in a comment
                } else if (lexer_peek(&lexer) == '*') {   // Multi line comment
                    lexer_read(&lexer);
                    lexer_skip_multiline_comment(&lexer);
                    vector_header(lexer.buffer)->length--;// We don't want to emit the slash if we are in a comment
                }
                break;
            }
            default:
                printf("Character: '%c' (%u)\n", character, (u8) character);
                break;
        }
    }

    Token *eof_token = vector_add(lexer.buffer, 1);
    eof_token->kind = TOKEN_EOF;
    eof_token->location = (Location){.file = lexer.filename, .line = lexer.line, .column = lexer.column};

    return lexer.buffer;
}
