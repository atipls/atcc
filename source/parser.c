#include "atcc.h"
#include "ati/utils.h"
#include <string.h>


#define make_ast(kind) make_ast_at(parser->current->location, kind)
static ASTNode *make_ast_at(Location location, ASTKind kind) {
    ASTNode *node = make(ASTNode);
    node->location = location;
    node->kind = kind;
    return node;
}

typedef struct {
    Token *tokens;
    Token *current;
    Token *previous;
} Parser;

static Token *parser_advance(Parser *parser) {
    parser->previous = parser->current;
    parser->current++;
    return parser->previous;
}

static Token *parser_check(Parser *parser, TokenKind kind) {
    if (parser->current->kind != kind)
        return null;
    return parser->current;
}

static Token *parser_consume(Parser *parser, TokenKind kind) {
    if (parser_check(parser, kind))
        return parser_advance(parser);
    return null;
}

static ASTNode *make_error(Parser *parser, string message) {
    ASTNode *node = make_ast(AST_ERROR);
    node->value = message;
    return node;
}

static ASTNode *parse_typedecl(Parser *parser);
static ASTNode *parse_expression(Parser *parser);
static ASTNode *parse_statement(Parser *parser);
static ASTNode *parse_declaration(Parser *parser);

ASTNode *parse_expression(Parser *parser) {
    return make_error(parser, str("Not implemented"));
}

ASTNode *parse_statement(Parser *parser) {
    return make_error(parser, str("Not implemented"));
}

static ASTNode *parse_typedecl_argument(Parser *parser) {
    ASTNode *param_type = parse_typedecl(parser);

    if (parser_check(parser, TOKEN_COLON)) {
        if (param_type->kind != AST_DECLARATION_TYPE_NAME)
            return make_error(parser, str("Expected type name after ':' in a type declaration."));
        param_type = parse_typedecl(parser);
    }

    return param_type;
}

static ASTNode *parse_typedecl_function(Parser *parser) {
    if (!parser_consume(parser, TOKEN_KW_FUN))
        return null;// Unreachable?
    if (!parser_consume(parser, TOKEN_OPEN_PAREN))
        return make_error(parser, str("Expected '(' after 'fun'."));

    ASTNode **parameters = null;
    bool is_variadic = false;
    if (!parser_check(parser, TOKEN_CLOSE_PAREN)) {
        do {
            vector_push(parameters, parse_typedecl_argument(parser));
            // TODO: Check for '...'
        } while (parser_consume(parser, TOKEN_COMMA));
    }

    if (!parser_consume(parser, TOKEN_CLOSE_PAREN))
        return make_error(parser, str("Expected ')' after function parameters."));

    ASTNode *return_type = null;
    if (parser_check(parser, TOKEN_COLON))
        return_type = parse_typedecl(parser);

    ASTNode *typedecl = make_ast(AST_DECLARATION_TYPE_FUNCTION);
    typedecl->function_parameters = parameters;
    typedecl->function_return_type = return_type;
    typedecl->function_is_variadic = is_variadic;
    return typedecl;
}

static ASTNode *parse_typedecl_base(Parser *parser) {
    if (parser_check(parser, TOKEN_KW_FUN))
        return parse_typedecl_function(parser);

    if (parser_consume(parser, TOKEN_IDENTIFIER)) {
        ASTNode *identifier = make_ast(AST_DECLARATION_TYPE_NAME);
        identifier->value = parser->previous->value;
        return identifier;
    }

    if (parser_check(parser, TOKEN_OPEN_PAREN)) {
        ASTNode *unwrapped = parse_typedecl(parser);
        if (!parser_consume(parser, TOKEN_CLOSE_PAREN))
            return make_error(parser, str("Expected ')' after type declaration."));
        return unwrapped;
    }

    return make_error(parser, str("Unexpected token in type declaration."));
}

static ASTNode *parse_typedecl(Parser *parser) {
    ASTNode *typedecl = parse_typedecl_base(parser);

    if (parser_check(parser, TOKEN_OPEN_BRACKET) || parser_check(parser, TOKEN_STAR)) {
        if (parser_consume(parser, TOKEN_OPEN_BRACKET)) {
            ASTNode *array_size = null;
            if (!parser_check(parser, TOKEN_CLOSE_BRACKET))
                array_size = parse_expression(parser);
            ASTNode *base_type = typedecl;
            typedecl = make_ast(AST_DECLARATION_TYPE_ARRAY);
            typedecl->array_base = base_type;
            typedecl->array_size = array_size;
        } else {
            if (!parser_consume(parser, TOKEN_STAR))
                return make_error(parser, str("Expected '*' after '[' in a type declaration."));
            ASTNode *base_type = typedecl;
            typedecl = make_ast(AST_DECLARATION_TYPE_POINTER);
            typedecl->parent = base_type;
        }
    }

    return typedecl;
}

static ASTNode *parse_declaration_alias(Parser *parser) {
    Token *new_name = parser_consume(parser, TOKEN_IDENTIFIER);
    if (new_name == null)
        return null;
    if (!parser_consume(parser, TOKEN_EQUAL))
        return make_error(parser, str("Expected '=' after 'alias name'."));
    ASTNode *node = make_ast(AST_DECLARATION_ALIAS);
    node->alias.name = new_name->value;
    node->alias.type = parse_typedecl(parser);
    if (!parser_consume(parser, TOKEN_SEMICOLON))
        return make_error(parser, str("Expected ';' after 'alias'."));
    return node;
}

static ASTNode *parse_declaration_aggregate(Parser *parser) {
    return make_error(parser, str("Not implemented"));
}

static ASTNode *parse_declaration_variable(Parser *parser) {
    return make_error(parser, str("Not implemented"));
}

static ASTNode *parse_declaration_function(Parser *parser) {
    Token *name = parser_consume(parser, TOKEN_IDENTIFIER);
    if (name == null)
        return make_error(parser, str("Expected identifier after 'fun'."));
    if (!parser_consume(parser, TOKEN_OPEN_PAREN))
        return make_error(parser, str("Expected '(' after function name."));

    ASTNode **parameters = null;
    bool is_variadic = false;
    if (!parser_check(parser, TOKEN_CLOSE_PAREN)) {
        do {
            // TODO: Check for '...'

        } while (parser_consume(parser, TOKEN_COMMA));
    }

    return make_error(parser, str("Not implemented"));
}

ASTNode *parse_declaration(Parser *parser) {
    if (parser_consume(parser, TOKEN_KW_ALIAS)) return parse_declaration_alias(parser);
    if (parser_consume(parser, TOKEN_KW_STRUCT)) return parse_declaration_aggregate(parser);
    if (parser_consume(parser, TOKEN_KW_UNION)) return parse_declaration_aggregate(parser);
    if (parser_consume(parser, TOKEN_KW_VAR)) return parse_declaration_variable(parser);
    if (parser_consume(parser, TOKEN_KW_CONST)) return parse_declaration_variable(parser);
    if (parser_consume(parser, TOKEN_KW_FUN)) return parse_declaration_function(parser);
    return null;
}

ASTNode *parse_program(Token *tokens) {
    Parser *parser = make(Parser);
    parser->tokens = tokens;
    parser->current = parser->tokens;
    parser->previous = null;

    ASTNode *program = make_ast(AST_PROGRAM);
    program->location = parser->current->location;

    while (!parser_check(parser, TOKEN_EOF)) {
        ASTNode *declaration = parse_declaration(parser);
        if (!declaration) {
            printf("Failed to parse declaration (Current is: %d '%.*s')\n", parser->current->kind, (i32) parser->current->value.length, parser->current->value.data);
            parser_advance(parser);
            continue;
        }

        vector_push(program->declarations, declaration);
    }

    return program;
}