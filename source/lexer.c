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

static void lexer_build_identifier(Lexer *lexer, Token *token) {
    static struct {
        string ident;
        TokenKind kind;
    } keywords[] = {
            {str("alias"), TOKEN_KW_ALIAS},
            {str("alignof"), TOKEN_KW_ALIGNOF},
            {str("break"), TOKEN_KW_BREAK},
            {str("case"), TOKEN_KW_CASE},
            {str("cast"), TOKEN_KW_CAST},
            {str("const"), TOKEN_KW_CONST},
            {str("continue"), TOKEN_KW_CONTINUE},
            {str("default"), TOKEN_KW_DEFAULT},
            {str("do"), TOKEN_KW_DO},
            {str("else"), TOKEN_KW_ELSE},
            {str("enum"), TOKEN_KW_ENUM},
            {str("for"), TOKEN_KW_FOR},
            {str("fun"), TOKEN_KW_FUN},
            {str("if"), TOKEN_KW_IF},
            {str("offsetof"), TOKEN_KW_OFFSETOF},
            {str("return"), TOKEN_KW_RETURN},
            {str("sizeof"), TOKEN_KW_SIZEOF},
            {str("struct"), TOKEN_KW_STRUCT},
            {str("switch"), TOKEN_KW_SWITCH},
            {str("union"), TOKEN_KW_UNION},
            {str("var"), TOKEN_KW_VAR},
            {str("while"), TOKEN_KW_WHILE},
    };

    i8 *buffer = null;
    while (isalnum(lexer_peek(lexer)) || lexer_peek(lexer) == '_')
        vector_push(buffer, lexer_read(lexer));

    token->kind = TOKEN_IDENTIFIER;
    token->value = rawstr(buffer, vector_len(buffer));
    for (i32 i = 0; i < array_length(keywords); i++) {
        if (string_match(token->value, keywords[i].ident)) {
            token->kind = keywords[i].kind;
            return;
        }
    }
}

static void lexer_build_number(Lexer *lexer, Token *token) {
    i8 *buffer = null, seen_dot = 0;
    while (isdigit(lexer_peek(lexer)) || lexer_peek(lexer) == '.')
        vector_push(buffer, lexer_read(lexer));

    token->kind = TOKEN_NUMBER;
    token->value = rawstr(buffer, vector_len(buffer));
}

static void lexer_build_string_or_char(Lexer *lexer, Token *token, i8 terminator) {
    i8 *buffer = null;
    while (lexer_peek(lexer) != terminator)
        vector_push(buffer, lexer_read(lexer));

    lexer_read(lexer);
    vector_push(buffer, '\0');

    token->kind = terminator == '"' ? TOKEN_STRING : TOKEN_CHAR;
    token->value = rawstr(buffer, vector_len(buffer));
}

static void lexer_build_equal(Lexer *lexer, Token *token, TokenKind token1, TokenKind token2) {
    token->kind = token1;
    if (lexer_peek(lexer) == '=') {
        lexer_read(lexer);
        token->kind = token2;
    }
}

Token *lexer_tokenize(string filename, Buffer data) {
    Lexer lexer = {0};
    lexer.filename = filename;
    lexer.source = data;
    lexer.line = 1;
    lexer.column = 1;

    while (lexer_peek(&lexer) != 0) {
        while (isspace(lexer_peek(&lexer)))
            lexer_read(&lexer);

        Token *current = vector_add(lexer.buffer, 1);
        current->kind = TOKEN_NONE;
        current->flags = TOKEN_FLAG_NONE;
        current->location = (Location){.file = lexer.filename, .line = lexer.line, .column = lexer.column};
        current->value = str("");

        if (lexer_peek(&lexer) == 0)
            current->kind = TOKEN_EOF;

        if (isalpha(lexer_peek(&lexer))) {
            lexer_build_identifier(&lexer, current);
            continue;
        }

        if (isnumber(lexer_peek(&lexer))) {
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
            case '/': lexer_build_equal(&lexer, current, TOKEN_SLASH, TOKEN_SLASH_EQUAL); break;
            case '%': lexer_build_equal(&lexer, current, TOKEN_PERCENT, TOKEN_PERCENT_EQUAL); break;
            case '^': lexer_build_equal(&lexer, current, TOKEN_CARET, TOKEN_CARET_EQUAL); break;
            case '~': lexer_build_equal(&lexer, current, TOKEN_TILDE, TOKEN_TILDE_EQUAL); break;
            case '=': lexer_build_equal(&lexer, current, TOKEN_EQUAL, TOKEN_EQUAL_EQUAL); break;
            case '!': lexer_build_equal(&lexer, current, TOKEN_EXCLAMATION, TOKEN_EXCLAMATION_EQUAL); break;
            case ':': lexer_build_equal(&lexer, current, TOKEN_COLON, TOKEN_COLON_EQUAL); break;
            case '.': current->kind = TOKEN_DOT; break;
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
            }
            case '|': {
                current->kind = TOKEN_PIPE;
                if (lexer_peek(&lexer) == '&') {
                    lexer_read(&lexer);
                    current->kind = TOKEN_PIPE_EQUAL;
                    if (lexer_peek(&lexer) == '=') {
                        lexer_read(&lexer);
                        current->kind = TOKEN_PIPE_PIPE;
                    }
                } else if (lexer_peek(&lexer) == '=') {
                    lexer_read(&lexer);
                    current->kind = TOKEN_PIPE_PIPE_EQUAL;
                }
            }
            default:
                printf("Character: '%c'\n", character);
                break;
        }
    }

    Token *eof_token = vector_add(lexer.buffer, 1);
    eof_token->kind = TOKEN_EOF;
    eof_token->location = (Location){.file = lexer.filename, .line = lexer.line, .column = lexer.column};

    return lexer.buffer;
}
