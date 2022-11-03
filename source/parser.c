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
static ASTNode *parse_statement_simple(Parser *parser);
static ASTNode *parse_statement(Parser *parser);
static ASTNode *parse_statement_block(Parser *parser);
static ASTNode *parse_declaration(Parser *parser);

static bool parser_is_unary(Parser *parser) {
    return parser->current->kind == TOKEN_PLUS ||
           parser->current->kind == TOKEN_MINUS ||
           parser->current->kind == TOKEN_STAR ||
           parser->current->kind == TOKEN_AMPERSAND ||
           parser->current->kind == TOKEN_EXCLAMATION ||
           parser->current->kind == TOKEN_TILDE;
}

static bool parser_is_mul(Parser *parser) {
    return parser->current->kind == TOKEN_STAR ||
           parser->current->kind == TOKEN_SLASH ||
           parser->current->kind == TOKEN_PERCENT ||
           parser->current->kind == TOKEN_AMPERSAND ||
           parser->current->kind == TOKEN_LEFT_SHIFT ||
           parser->current->kind == TOKEN_RIGHT_SHIFT;
}

static bool parser_is_add(Parser *parser) {
    return parser->current->kind == TOKEN_PLUS ||
           parser->current->kind == TOKEN_MINUS ||
           parser->current->kind == TOKEN_CARET ||
           parser->current->kind == TOKEN_PIPE;
}

static bool parser_is_cmp(Parser *parser) {
    return parser->current->kind == TOKEN_EQUAL_EQUAL ||
           parser->current->kind == TOKEN_EXCLAMATION_EQUAL ||
           parser->current->kind == TOKEN_GREATER ||
           parser->current->kind == TOKEN_GREATER_EQUAL ||
           parser->current->kind == TOKEN_LESS ||
           parser->current->kind == TOKEN_LESS_EQUAL ||
           parser->current->kind == TOKEN_AMPERSAND_AMPERSAND ||
           parser->current->kind == TOKEN_PIPE_PIPE;
}

static bool parser_is_assign(Parser *parser) {
    return parser->current->kind == TOKEN_PLUS_EQUAL ||
           parser->current->kind == TOKEN_MINUS_EQUAL ||
           parser->current->kind == TOKEN_STAR_EQUAL ||
           parser->current->kind == TOKEN_SLASH_EQUAL ||
           parser->current->kind == TOKEN_PERCENT_EQUAL ||
           parser->current->kind == TOKEN_AMPERSAND_EQUAL ||
           parser->current->kind == TOKEN_PIPE_EQUAL ||
           parser->current->kind == TOKEN_CARET_EQUAL ||
           parser->current->kind == TOKEN_TILDE_EQUAL ||
           parser->current->kind == TOKEN_AMPERSAND_AMPERSAND_EQUAL ||
           parser->current->kind == TOKEN_PIPE_PIPE_EQUAL ||
           parser->current->kind == TOKEN_LEFT_SHIFT_EQUAL ||
           parser->current->kind == TOKEN_RIGHT_SHIFT_EQUAL ||
           parser->current->kind == TOKEN_EQUAL;
}

static ASTNode *parse_expression_compound_field(Parser *parser) {
    if (parser_consume(parser, TOKEN_OPEN_BRACKET)) {
        ASTNode *compound_index = parse_expression(parser);
        if (!parser_consume(parser, TOKEN_CLOSE_BRACKET))
            return make_error(parser, str("Expected ']'"));
        if (!parser_consume(parser, TOKEN_EQUAL))
            return make_error(parser, str("Expected '='"));
        ASTNode *node = make_ast(AST_EXPRESSION_COMPOUND_FIELD_INDEX);
        node->compound_field_index = compound_index;
        node->compound_field_target = parse_expression(parser);
        return node;
    } else {
        ASTNode *target_or_init = parse_expression(parser);
        if (parser_consume(parser, TOKEN_EQUAL)) {
            if (target_or_init->kind != AST_EXPRESSION_IDENTIFIER)
                return make_error(parser, str("Expected identifier for compound field assignment"));
            ASTNode *node = make_ast(AST_EXPRESSION_COMPOUND_FIELD_NAME);
            node->compound_field_name = target_or_init->value;
            node->compound_field_target = parse_expression(parser);
            return node;
        } else {
            ASTNode *node = make_ast(AST_EXPRESSION_COMPOUND_FIELD);
            node->compound_field_target = target_or_init;
            return node;
        }
    }
}

static ASTNode *parse_expression_compound(Parser *parser, ASTNode *type) {
    if (!parser_consume(parser, TOKEN_OPEN_BRACE))
        return make_error(parser, str("Expected '{' in compound expression"));
    ASTNode **fields = null;
    while (!parser_check(parser, TOKEN_CLOSE_BRACE)) {
        vector_push(fields, parse_expression_compound_field(parser));
        if (!parser_consume(parser, TOKEN_COMMA))
            break;
    }
    if (!parser_consume(parser, TOKEN_CLOSE_BRACE))
        return make_error(parser, str("Expected '}' after compound expression"));
    ASTNode *node = make_ast(AST_EXPRESSION_COMPOUND);
    node->compound_type = type;
    node->compound_fields = fields;
    return node;
}

static ASTNode *parse_expression_unary(Parser *parser);
static ASTNode *parse_expression_operand(Parser *parser) {
    if (parser_check(parser, TOKEN_NUMBER) || parser_check(parser, TOKEN_STRING) || parser_check(parser, TOKEN_CHAR)) {
        Token *token = parser_advance(parser);
        ASTKind node_kind = token->kind == TOKEN_NUMBER ? AST_EXPRESSION_LITERAL_NUMBER : token->kind == TOKEN_STRING ? AST_EXPRESSION_LITERAL_STRING
                                                                                                                      : AST_EXPRESSION_LITERAL_CHAR;
        ASTNode *node = make_ast(node_kind);
        node->literal_value = token->value;
        node->literal_flags = token->flags;
        return node;
    }

    if (parser_check(parser, TOKEN_IDENTIFIER)) {
        Token *token = parser_advance(parser);
        if (parser_check(parser, TOKEN_OPEN_BRACE)) {
            ASTNode *compound_type = make_ast(AST_DECLARATION_TYPE_NAME);
            compound_type->value = token->value;
            return parse_expression_compound(parser, compound_type);
        }

        ASTNode *node = make_ast(AST_EXPRESSION_IDENTIFIER);
        node->value = token->value;
        return node;
    }

    if (parser_check(parser, TOKEN_OPEN_BRACE))
        return parse_expression_compound(parser, null);

    if (parser_consume(parser, TOKEN_OPEN_PAREN)) {
        if (parser_consume(parser, TOKEN_COLON)) {
            ASTNode *type = parse_typedecl(parser);
            if (!parser_consume(parser, TOKEN_CLOSE_PAREN))
                return make_error(parser, str("Expected ')'"));
            if (!parser_consume(parser, TOKEN_OPEN_BRACE))
                return make_error(parser, str("Expected '{'"));
            return parse_expression_compound(parser, type);
        } else {
            ASTNode *node = make_ast(AST_EXPRESSION_PAREN);
            node->parent = parse_expression(parser);
            if (!parser_consume(parser, TOKEN_CLOSE_PAREN))
                return make_error(parser, str("Expected ')' after expression"));
            return node;
        }
    }

    if (parser_consume(parser, TOKEN_KW_CAST)) {
        if (!parser_consume(parser, TOKEN_OPEN_PAREN))
            return make_error(parser, str("Expected '(' after 'cast'"));
        ASTNode *target_type = parse_typedecl(parser);
        if (!parser_consume(parser, TOKEN_CLOSE_PAREN))
            return make_error(parser, str("Expected ')' after 'cast'"));
        ASTNode *node = make_ast(AST_EXPRESSION_CAST);
        node->cast_type = target_type;
        node->cast_target = parse_expression_unary(parser);
        return node;
    }

    return make_error(parser, str("Unexpected token"));
}

static ASTNode *parse_expression_base(Parser *parser) {
    ASTNode *node = parse_expression_operand(parser);
    while (parser_check(parser, TOKEN_OPEN_PAREN) || parser_check(parser, TOKEN_OPEN_BRACKET) || parser_check(parser, TOKEN_DOT)) {
        if (parser_consume(parser, TOKEN_OPEN_PAREN)) {
            ASTNode **arguments = null;
            if (!parser_check(parser, TOKEN_CLOSE_PAREN)) {
                do {
                    vector_push(arguments, parse_expression(parser));
                } while (parser_consume(parser, TOKEN_COMMA));
            }
            if (!parser_consume(parser, TOKEN_CLOSE_PAREN))
                return make_error(parser, str("Expected ')' after function call."));
            ASTNode *new_node = make_ast(AST_EXPRESSION_CALL);
            new_node->call_target = node;
            new_node->call_arguments = arguments;
            node = new_node;
        } else if (parser_consume(parser, TOKEN_OPEN_BRACKET)) {
            ASTNode *array_index = parse_expression(parser);
            if (!parser_consume(parser, TOKEN_CLOSE_BRACKET))
                return make_error(parser, str("Expected ']' after array index."));
            ASTNode *new_node = make_ast(AST_EXPRESSION_INDEX);
            new_node->index_target = node;
            new_node->index_index = array_index;
            node = new_node;
        } else if (parser_consume(parser, TOKEN_DOT)) {
            if (!parser_consume(parser, TOKEN_IDENTIFIER))
                return make_error(parser, str("Expected identifier after '.'"));
            ASTNode *new_node = make_ast(AST_EXPRESSION_FIELD);
            new_node->field_target = node;
            new_node->field_name = parser->previous->value;
            node = new_node;
        }
    }

    return node;
}

static ASTNode *parse_expression_unary(Parser *parser) {
    if (parser_is_unary(parser)) {
        ASTNode *node = make_ast(AST_EXPRESSION_UNARY);
        node->unary_operator = parser_advance(parser)->kind;
        node->unary_target = parse_expression_unary(parser);
        return node;
    }
    return parse_expression_base(parser);
}

static ASTNode *parse_expression_mul(Parser *parser) {
    ASTNode *node = parse_expression_unary(parser);
    while (parser_is_mul(parser)) {
        TokenKind binop = parser_advance(parser)->kind;
        ASTNode *new_node = make_ast(AST_EXPRESSION_BINARY);
        new_node->binary_operator = binop;
        new_node->binary_left = node;
        new_node->binary_right = parse_expression_unary(parser);
        node = new_node;
    }
    return node;
}

static ASTNode *parse_expression_add(Parser *parser) {
    ASTNode *node = parse_expression_mul(parser);
    while (parser_is_add(parser)) {
        TokenKind binop = parser_advance(parser)->kind;
        ASTNode *new_node = make_ast(AST_EXPRESSION_BINARY);
        new_node->binary_operator = binop;
        new_node->binary_left = node;
        new_node->binary_right = parse_expression_mul(parser);
        node = new_node;
    }
    return node;
}

static ASTNode *parse_expression_cmp(Parser *parser) {
    ASTNode *node = parse_expression_add(parser);
    while (parser_is_cmp(parser)) {
        TokenKind binop = parser_advance(parser)->kind;
        ASTNode *new_node = make_ast(AST_EXPRESSION_BINARY);
        new_node->binary_operator = binop;
        new_node->binary_left = node;
        new_node->binary_right = parse_expression_add(parser);
        node = new_node;
    }
    return node;
}

static ASTNode *parse_expression_and(Parser *parser) {
    ASTNode *node = parse_expression_cmp(parser);
    while (parser_consume(parser, TOKEN_AMPERSAND_AMPERSAND)) {
        ASTNode *new_node = make_ast(AST_EXPRESSION_BINARY);
        new_node->binary_operator = TOKEN_AMPERSAND_AMPERSAND;
        new_node->binary_left = node;
        new_node->binary_right = parse_expression_cmp(parser);
        node = new_node;
    }
    return node;
}

static ASTNode *parse_expression_or(Parser *parser) {
    ASTNode *node = parse_expression_and(parser);
    while (parser_consume(parser, TOKEN_PIPE_PIPE)) {
        ASTNode *new_node = make_ast(AST_EXPRESSION_BINARY);
        new_node->binary_operator = TOKEN_PIPE_PIPE;
        new_node->binary_left = node;
        new_node->binary_right = parse_expression_and(parser);
        node = new_node;
    }
    return node;
}

ASTNode *parse_expression(Parser *parser) {
    ASTNode *node = parse_expression_or(parser);
    if (parser_consume(parser, TOKEN_QUESTION)) {
        ASTNode *ternary_true = parse_expression(parser);
        if (!parser_consume(parser, TOKEN_COLON))
            return make_error(parser, str("Expected ':' after ternary expression"));
        ASTNode *ternary_false = parse_expression(parser);
        ASTNode *new_node = make_ast(AST_EXPRESSION_TERNARY);
        new_node->ternary_condition = node;
        new_node->ternary_true = ternary_true;
        new_node->ternary_false = ternary_false;
        node = new_node;
    }
    return node;
}

static ASTNode *parse_expression_paren(Parser *parser) {
    if (!parser_consume(parser, TOKEN_OPEN_PAREN))
        return make_error(parser, str("Expected '('"));
    ASTNode *node = parse_expression(parser);
    if (!parser_consume(parser, TOKEN_CLOSE_PAREN))
        return make_error(parser, str("Expected ')'"));
    return node;
}

static ASTNode *parse_statement_init(Parser *parser, ASTNode *expression) {
    if (parser_consume(parser, TOKEN_COLON_EQUAL)) {
        if (expression->kind != AST_EXPRESSION_IDENTIFIER)
            return make_error(parser, str(":= must be used on an identifier"));
        ASTNode *node = make_ast(AST_STATEMENT_INIT);
        node->init_name = expression->value;
        node->init_value = parse_expression(parser);
        return node;
    }

    if (parser_consume(parser, TOKEN_COLON)) {
        if (expression->kind != AST_EXPRESSION_IDENTIFIER)
            return make_error(parser, str(": must be used on an identifier"));
        ASTNode *node = make_ast(AST_STATEMENT_INIT);
        node->init_name = expression->value;
        node->init_type = parse_typedecl(parser);
        if (parser_consume(parser, TOKEN_EQUAL)) {
            node->init_is_nothing = parser_consume(parser, TOKEN_DOT_DOT) != null;
            if (!node->init_is_nothing)
                node->init_value = parse_expression(parser);
        }
        return node;
    }

    return null;
}

static ASTNode *parse_statement_if(Parser *parser) {
    if (!parser_consume(parser, TOKEN_OPEN_PAREN))
        return make_error(parser, str("Expected '(' after 'if'"));
    ASTNode *condition = parse_expression(parser);
    ASTNode *expression = parse_statement_init(parser, condition);
    if (expression) condition = parser_consume(parser, TOKEN_SEMICOLON) ? parse_expression(parser) : null;
    if (!parser_consume(parser, TOKEN_CLOSE_PAREN))
        return make_error(parser, str("Expected ')' after 'if' condition"));

    ASTNode *if_true = parse_statement_block(parser);
    ASTNode *if_false = parser_consume(parser, TOKEN_KW_ELSE) ? parse_statement_block(parser) : null;

    ASTNode *node = make_ast(AST_STATEMENT_IF);
    node->if_expression = expression;
    node->if_condition = condition;
    node->if_true = if_true;
    node->if_false = if_false;
    return node;
}

static ASTNode *parse_statement_while(Parser *parser) {
    ASTNode *node = make_ast(AST_STATEMENT_WHILE);
    node->while_condition = parse_expression_paren(parser);
    node->while_body = parse_statement_block(parser);
    return node;
}

static ASTNode *parse_statement_do_while(Parser *parser) {
    ASTNode *body = parse_statement_block(parser);
    if (!parser_consume(parser, TOKEN_KW_WHILE))
        return make_error(parser, str("Expected 'while' after 'do'"));
    ASTNode *condition = parse_expression_paren(parser);
    if (!parser_consume(parser, TOKEN_SEMICOLON))
        return make_error(parser, str("Expected ';' after 'while' condition"));
    ASTNode *node = make_ast(AST_STATEMENT_DO_WHILE);
    node->while_condition = condition;
    node->while_body = body;
    return node;
}

static ASTNode *parse_statement_for(Parser *parser) {
    ASTNode *initializer = null;
    ASTNode *condition = null;
    ASTNode *increment = null;

    if (!parser_check(parser, TOKEN_OPEN_BRACE)) {
        if (!parser_consume(parser, TOKEN_OPEN_PAREN))
            return make_error(parser, str("Expected '(' after 'for'"));
        if (!parser_check(parser, TOKEN_SEMICOLON))
            initializer = parse_statement_simple(parser);
        if (parser_consume(parser, TOKEN_SEMICOLON)) {
            if (!parser_check(parser, TOKEN_SEMICOLON))
                condition = parse_expression(parser);
            if (parser_consume(parser, TOKEN_SEMICOLON)) {
                if (!parser_check(parser, TOKEN_CLOSE_PAREN)) {
                    increment = parse_statement_simple(parser);
                    if (increment->kind == AST_STATEMENT_INIT)
                        return make_error(parser, str("Initializers are not allowed in for's increment"));
                }
            }
        }

        if (!parser_consume(parser, TOKEN_CLOSE_PAREN))
            return make_error(parser, str("Expected ')' after 'for' condition"));
    }

    ASTNode *node = make_ast(AST_STATEMENT_FOR);
    node->for_initializer = initializer;
    node->for_condition = condition;
    node->for_increment = increment;
    node->for_body = parse_statement_block(parser);
    return node;
}

static ASTNode *parse_statement_switch_case(Parser *parser) {
    ASTNode **switch_patterns = null;
    bool is_default = false;
    while (parser_check(parser, TOKEN_KW_CASE) || parser_check(parser, TOKEN_KW_DEFAULT)) {
        if (parser_consume(parser, TOKEN_KW_CASE)) {
            do {
                ASTNode *pattern = make_ast(AST_STATEMENT_SWITCH_PATTERN);
                pattern->switch_pattern_start = parse_expression(parser);
                if (parser_consume(parser, TOKEN_DOT_DOT))
                    pattern->switch_pattern_end = parse_expression(parser);
                vector_push(switch_patterns, pattern);
            } while (parser_consume(parser, TOKEN_COMMA));
        } else {
            parser_consume(parser, TOKEN_KW_DEFAULT);
            if (is_default) return make_error(parser, str("Multiple default clauses."));
            is_default = true;
        }

        if (!parser_consume(parser, TOKEN_COLON))
            return make_error(parser, str("Expected ':' after switch value(s)."));
    }

    ASTNode *node = make_ast(AST_STATEMENT_SWITCH_CASE);
    node->switch_case_patterns = switch_patterns;
    node->switch_case_body = make_ast(AST_STATEMENT_BLOCK);
    node->switch_case_is_default = is_default;
    while (!parser_check(parser, TOKEN_EOF) && !parser_check(parser, TOKEN_CLOSE_BRACE) && !parser_check(parser, TOKEN_KW_CASE) && !parser_check(parser, TOKEN_KW_DEFAULT))
        vector_push(node->switch_case_body->statements, parse_statement(parser));
    return node;
}

static ASTNode *parse_statement_switch(Parser *parser) {
    ASTNode *expression = parse_expression_paren(parser);
    if (!parser_consume(parser, TOKEN_OPEN_BRACE))
        return make_error(parser, str("Expected '{' after 'switch'"));

    ASTNode **switch_cases = null;
    while (!parser_check(parser, TOKEN_CLOSE_BRACE) && !parser_check(parser, TOKEN_EOF))
        vector_push(switch_cases, parse_statement_switch_case(parser));

    if (!parser_consume(parser, TOKEN_CLOSE_BRACE))
        return make_error(parser, str("Expected '}' after block"));

    ASTNode *node = make_ast(AST_STATEMENT_SWITCH);
    node->switch_expression = expression;
    node->switch_cases = switch_cases;

    return node;
}

static ASTNode *parse_statement_break_continue(Parser *parser, TokenKind kind) {
    ASTNode *node = make_ast(kind == TOKEN_KW_BREAK ? AST_STATEMENT_BREAK : AST_STATEMENT_CONTINUE);
    if (parser_consume(parser, TOKEN_SEMICOLON))
        return node;

    if (kind == TOKEN_KW_BREAK)
        return make_error(parser, str("Expected ';' after 'break'"));
    return make_error(parser, str("Expected ';' after 'continue'"));
}

static ASTNode *parse_statement_return(Parser *parser) {
    ASTNode *node = make_ast(AST_STATEMENT_RETURN);
    if (parser_check(parser, TOKEN_SEMICOLON))
        return node;
    node->parent = parse_expression(parser);
    if (!parser_consume(parser, TOKEN_SEMICOLON))
        return make_error(parser, str("Expected ';' after 'return'"));
    return node;
}

static ASTNode *parse_statement_simple(Parser *parser) {
    ASTNode *expression = parse_expression(parser);
    ASTNode *statement = parse_statement_init(parser, expression);
    if (!statement) {
        if (parser_is_assign(parser)) {
            TokenKind assign = parser_advance(parser)->kind;
            statement = make_ast(AST_STATEMENT_ASSIGN);
            statement->assign_operator = assign;
            statement->assign_target = expression;
            statement->assign_value = parse_expression(parser);
        } else {
            statement = make_ast(AST_STATEMENT_EXPRESSION);
            statement->parent = expression;
        }
    }

    return statement;
}

static ASTNode *parse_statement(Parser *parser) {
    if (parser_consume(parser, TOKEN_KW_IF))
        return parse_statement_if(parser);
    if (parser_consume(parser, TOKEN_KW_WHILE))
        return parse_statement_while(parser);
    if (parser_consume(parser, TOKEN_KW_DO))
        return parse_statement_do_while(parser);
    if (parser_consume(parser, TOKEN_KW_FOR))
        return parse_statement_for(parser);
    if (parser_consume(parser, TOKEN_KW_SWITCH))
        return parse_statement_switch(parser);
    if (parser_consume(parser, TOKEN_KW_BREAK))
        return parse_statement_break_continue(parser, TOKEN_KW_BREAK);
    if (parser_consume(parser, TOKEN_KW_CONTINUE))
        return parse_statement_break_continue(parser, TOKEN_KW_CONTINUE);
    if (parser_consume(parser, TOKEN_KW_RETURN))
        return parse_statement_return(parser);
    if (parser_check(parser, TOKEN_OPEN_BRACE))
        return parse_statement_block(parser);
    ASTNode *simple = parse_statement_simple(parser);
    if (!parser_consume(parser, TOKEN_SEMICOLON))
        return make_error(parser, str("Expected ';' after statement"));
    return simple;
}

static ASTNode *parse_statement_block(Parser *parser) {
    if (parser_consume(parser, TOKEN_OPEN_BRACE)) {
        ASTNode *node = make_ast(AST_STATEMENT_BLOCK);
        while (!parser_check(parser, TOKEN_CLOSE_BRACE) && !parser_check(parser, TOKEN_EOF))
            vector_push(node->statements, parse_statement(parser));
        if (!parser_consume(parser, TOKEN_CLOSE_BRACE))
            return make_error(parser, str("Expected '}' after block"));
        return node;
    }
    return parse_statement(parser);
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

    while (parser_check(parser, TOKEN_OPEN_BRACKET) || parser_check(parser, TOKEN_STAR)) {
        if (parser_consume(parser, TOKEN_OPEN_BRACKET)) {
            ASTNode *array_size = null;
            bool array_is_dynamic = false;

            if (parser_consume(parser, TOKEN_STAR))
                array_is_dynamic = true;
            else if (!parser_check(parser, TOKEN_CLOSE_BRACKET))
                array_size = parse_expression(parser);

            if (!parser_consume(parser, TOKEN_CLOSE_BRACKET))
                return make_error(parser, str("Expected ']' after array size in a type."));

            ASTNode *base_type = typedecl;
            typedecl = make_ast(AST_DECLARATION_TYPE_ARRAY);
            typedecl->array_base = base_type;
            typedecl->array_size = array_size;
            typedecl->array_is_dynamic = array_is_dynamic;
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
    node->alias_name = new_name->value;
    node->alias_type = parse_typedecl(parser);
    if (!parser_consume(parser, TOKEN_SEMICOLON))
        return make_error(parser, str("Expected ';' after 'alias'."));
    return node;
}

static ASTNode *parse_aggregate(Parser *parser, TokenKind kind);
static ASTNode *parse_aggregate_item(Parser *parser) {
    if (parser_consume(parser, TOKEN_KW_STRUCT) || parser_consume(parser, TOKEN_KW_UNION)) {
        ASTNode *node = make_ast(AST_DECLARATION_AGGREGATE_CHILD);
        node->aggregate_kind = parser->previous->kind;
        vector_push(node->aggregate_items, parse_aggregate(parser, node->aggregate_kind));
        return node;
    }

    ASTNode *node = make_ast(AST_DECLARATION_AGGREGATE_FIELD);
    do {
        if (!parser_consume(parser, TOKEN_IDENTIFIER))
            return make_error(parser, str("Expected an identifier as a field name."));
        vector_push(node->aggregate_names, parser->previous->value);
    } while (parser_consume(parser, TOKEN_COMMA));

    if (!parser_consume(parser, TOKEN_COLON))
        return make_error(parser, str("Expected ':' after field names."));

    node->aggregate_type = parse_typedecl(parser);

    if (!parser_consume(parser, TOKEN_SEMICOLON))
        return make_error(parser, str("Expected ';' after field type."));

    return node;
}

static ASTNode *parse_aggregate(Parser *parser, TokenKind kind) {
    if (!parser_consume(parser, TOKEN_OPEN_BRACE))
        return make_error(parser, str("Expected '{' after aggregate."));

    ASTNode *node = make_ast(AST_DECLARATION_AGGREGATE_CHILD);
    node->aggregate_kind = kind;
    while (!parser_check(parser, TOKEN_EOF) && !parser_check(parser, TOKEN_CLOSE_BRACE))
        vector_push(node->aggregate_items, parse_aggregate_item(parser));

    if (!parser_consume(parser, TOKEN_CLOSE_BRACE))
        return make_error(parser, str("Expected '}' after aggregate."));

    return node;
}

static ASTNode *parse_declaration_aggregate(Parser *parser) {
    TokenKind kind = parser->previous->kind;
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
        return make_error(parser, str("Expected identifier after 'struct' or 'union'."));

    ASTNode *node = make_ast(AST_DECLARATION_AGGREGATE);
    node->aggregate_name = parser->previous->value;
    node->aggregate = parse_aggregate(parser, kind);

    return node;
}

static ASTNode *parse_declaration_variable(Parser *parser) {
    Token *variable_kind = parser->previous;
    ASTNode *node = make_ast(AST_DECLARATION_VARIABLE);
    node->variable_is_const = variable_kind->kind == TOKEN_KW_CONST;
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
        return make_error(parser, str("Expected identifier after 'var' or 'const'."));
    node->variable_name = parser->previous->value;

    if (parser_consume(parser, TOKEN_COLON_EQUAL)) {
        node->variable_type = null;
        node->variable_initializer = parse_expression(parser);
    } else if (parser_consume(parser, TOKEN_COLON)) {
        node->variable_type = parse_typedecl(parser);
        if (parser_consume(parser, TOKEN_EQUAL))
            node->variable_initializer = parse_expression(parser);
    }

    if (!parser_consume(parser, TOKEN_SEMICOLON))
        return make_error(parser, str("Expected ';' after 'var' or 'const' declaration."));

    return node;
}

static ASTNode *parse_declaration_function_parameter(Parser *parser) {
    ASTNode *node = make_ast(AST_DECLARATION_FUNCTION_PARAMETER);
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
        return make_error(parser, str("Expected identifier for 'function parameter'."));
    node->function_parameter_name = parser->previous->value;
    if (!parser_consume(parser, TOKEN_COLON))
        return make_error(parser, str("Expected ':' after 'function parameter'."));
    node->function_parameter_type = parse_typedecl(parser);
    return node;
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
        vector_push(parameters, parse_declaration_function_parameter(parser));
        while (parser_consume(parser, TOKEN_COMMA)) {
            if (parser_consume(parser, TOKEN_DOT_DOT)) {
                if (is_variadic)
                    return make_error(parser, str("Variadic parameter can only be declared once."));
                is_variadic = true;
            } else {
                if (is_variadic)
                    return make_error(parser, str("Variadic parameter must be the last parameter."));
                vector_push(parameters, parse_declaration_function_parameter(parser));
            }
        }
    }

    if (!parser_consume(parser, TOKEN_CLOSE_PAREN))
        return make_error(parser, str("Expected ')' after function parameters."));

    ASTNode *return_type = null;
    if (parser_consume(parser, TOKEN_COLON))
        return_type = parse_typedecl(parser);

    ASTNode *node = make_ast(AST_DECLARATION_FUNCTION);
    node->function_name = name->value;
    node->function_parameters = parameters;
    node->function_return_type = return_type;
    node->function_body = parser_consume(parser, TOKEN_SEMICOLON) ? null : parse_statement_block(parser);
    return node;
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

        if (declaration->kind == AST_ERROR)
            printf("Decl error: %.*s\n", (i32) declaration->value.length, declaration->value.data);

        vector_push(program->declarations, declaration);
    }

    return program;
}
