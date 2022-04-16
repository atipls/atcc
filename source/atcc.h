#pragma once

#include "ati/basic.h"
#include "ati/string.h"

typedef struct {
    string file;
    int line;
    int column;
} Location;

typedef enum {
    TOKEN_NONE,
    TOKEN_EOF,
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_CHAR,

    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,
    TOKEN_AMPERSAND,
    TOKEN_PIPE,
    TOKEN_CARET,
    TOKEN_TILDE,
    TOKEN_EQUAL,
    TOKEN_EXCLAMATION,
    TOKEN_COLON,
    TOKEN_LESS,
    TOKEN_GREATER,
    TOKEN_DOT,
    TOKEN_COMMA,
    TOKEN_QUESTION,
    TOKEN_OPEN_PAREN,
    TOKEN_CLOSE_PAREN,
    TOKEN_OPEN_BRACKET,
    TOKEN_CLOSE_BRACKET,
    TOKEN_OPEN_BRACE,
    TOKEN_CLOSE_BRACE,
    TOKEN_HASH,
    TOKEN_DOLLAR,
    TOKEN_AT,
    TOKEN_AMPERSAND_AMPERSAND,
    TOKEN_PIPE_PIPE,
    TOKEN_LEFT_SHIFT,
    TOKEN_RIGHT_SHIFT,
    TOKEN_SEMICOLON,

    TOKEN_PLUS_EQUAL,
    TOKEN_MINUS_EQUAL,
    TOKEN_STAR_EQUAL,
    TOKEN_SLASH_EQUAL,
    TOKEN_PERCENT_EQUAL,
    TOKEN_AMPERSAND_EQUAL,
    TOKEN_PIPE_EQUAL,
    TOKEN_CARET_EQUAL,
    TOKEN_TILDE_EQUAL,
    TOKEN_AMPERSAND_AMPERSAND_EQUAL,
    TOKEN_PIPE_PIPE_EQUAL,
    TOKEN_LEFT_SHIFT_EQUAL,
    TOKEN_RIGHT_SHIFT_EQUAL,

    TOKEN_EQUAL_EQUAL,
    TOKEN_EXCLAMATION_EQUAL,
    TOKEN_COLON_EQUAL,
    TOKEN_LESS_EQUAL,
    TOKEN_GREATER_EQUAL,

    TOKEN_KW_ALIAS,
    TOKEN_KW_ALIGNOF,
    TOKEN_KW_BREAK,
    TOKEN_KW_CASE,
    TOKEN_KW_CAST,
    TOKEN_KW_CONST,
    TOKEN_KW_CONTINUE,
    TOKEN_KW_DEFAULT,
    TOKEN_KW_DO,
    TOKEN_KW_ELSE,
    TOKEN_KW_ENUM,
    TOKEN_KW_FOR,
    TOKEN_KW_FUN,
    TOKEN_KW_IF,
    TOKEN_KW_OFFSETOF,
    TOKEN_KW_RETURN,
    TOKEN_KW_SIZEOF,
    TOKEN_KW_STRUCT,
    TOKEN_KW_SWITCH,
    TOKEN_KW_UNION,
    TOKEN_KW_VAR,
    TOKEN_KW_WHILE,
} TokenKind;

typedef enum {
    TOKEN_FLAG_NONE = 0,
    TOKEN_FLAG_HEX = 1 << 0,
    TOKEN_FLAG_OCT = 1 << 1,
    TOKEN_FLAG_BIN = 1 << 2,
    TOKEN_FLAG_FLOAT = 1 << 3,
} TokenFlag;

typedef struct {
    TokenKind kind;
    TokenFlag flags;
    Location location;
    string value;
} Token;

Token *lexer_tokenize(string filename, Buffer data);

typedef enum {
    AST_NONE,
    AST_ERROR,
    AST_PROGRAM,
    AST_DECLARATION_TYPE_NAME,
    AST_DECLARATION_TYPE_POINTER,
    AST_DECLARATION_TYPE_ARRAY,
    AST_DECLARATION_TYPE_FUNCTION,
    AST_DECLARATION_ALIAS,
    AST_DECLARATION_AGGREGATE,
    AST_DECLARATION_VARIABLE,
    AST_DECLARATION_FUNCTION,
} ASTKind;

typedef struct ASTNode ASTNode;
struct ASTNode {
    ASTKind kind;
    Location location;

    union {
        string value;
        ASTNode *parent;
        ASTNode **declarations;

        struct {
            ASTNode *array_base;
            ASTNode *array_size;
        };

        struct {
            ASTNode **function_parameters;
            ASTNode *function_return_type;
            bool function_is_variadic;
        };

        struct {
            string name;
            ASTNode *type;
        } alias;
    };
};

ASTNode *parse_program(Token *tokens);