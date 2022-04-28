#include "atcc.h"
#include "ati/utils.h"

static cstring ast_node_names[] = {
        [AST_NONE] = "none",
        [AST_ERROR] = "error",
        [AST_PROGRAM] = "program",
        [AST_DECLARATION_TYPE_NAME] = "declaration_type_name",
        [AST_DECLARATION_TYPE_POINTER] = "declaration_type_pointer",
        [AST_DECLARATION_TYPE_ARRAY] = "declaration_type_array",
        [AST_DECLARATION_TYPE_FUNCTION] = "declaration_type_function",
        [AST_DECLARATION_ALIAS] = "declaration_alias",
        [AST_DECLARATION_AGGREGATE] = "declaration_aggregate",
        [AST_DECLARATION_VARIABLE] = "declaration_variable",
        [AST_DECLARATION_FUNCTION_PARAMETER] = "declaration_function_parameter",
        [AST_DECLARATION_FUNCTION] = "declaration_function",
        [AST_STATEMENT_BLOCK] = "statement_block",
        [AST_STATEMENT_IF] = "statement_if",
        [AST_STATEMENT_WHILE] = "statement_while",
        [AST_STATEMENT_DO_WHILE] = "statement_do_while",
        [AST_STATEMENT_FOR] = "statement_for",
        [AST_STATEMENT_SWITCH] = "statement_switch",
        [AST_STATEMENT_BREAK] = "statement_break",
        [AST_STATEMENT_CONTINUE] = "statement_continue",
        [AST_STATEMENT_RETURN] = "statement_return",
        [AST_STATEMENT_INIT] = "statement_init",
        [AST_STATEMENT_EXPRESSION] = "statement_expression",
        [AST_STATEMENT_ASSIGN] = "statement_assign",
        [AST_EXPRESSION_PAREN] = "expression_paren",
        [AST_EXPRESSION_UNARY] = "expression_unary",
        [AST_EXPRESSION_BINARY] = "expression_binary",
        [AST_EXPRESSION_TERNARY] = "expression_ternary",
        [AST_EXPRESSION_LITERAL_NUMBER] = "expression_literal_number",
        [AST_EXPRESSION_LITERAL_CHAR] = "expression_literal_char",
        [AST_EXPRESSION_LITERAL_STRING] = "expression_literal_string",
        [AST_EXPRESSION_IDENTIFIER] = "expression_identifier",
        [AST_EXPRESSION_CALL] = "expression_call",
        [AST_EXPRESSION_FIELD] = "expression_field",
        [AST_EXPRESSION_INDEX] = "expression_index",
        [AST_EXPRESSION_CAST] = "expression_cast",
        [AST_EXPRESSION_COMPOUND] = "expression_compound",
        [AST_EXPRESSION_COMPOUND_FIELD] = "expression_compound_field",
        [AST_EXPRESSION_COMPOUND_FIELD_NAME] = "expression_compound_field_name",
        [AST_EXPRESSION_COMPOUND_FIELD_INDEX] = "expression_compound_field_index",
};

static void *write_string_dot(string s, FILE *f) {
    fprintf(f, "\"%p\" [label=\"%.*s\"]\n", s.data, strp(s));
    return s.data;
}

extern void fprint_type(FILE *f, Type *type);
static void write_node_name(ASTNode *node, FILE *f) {
    fprintf(f, "\"%p\" [label=\"", node);

    switch (node->kind) {
        case AST_ERROR:
            fprintf(f, "error: '%.*s'", strp(node->value));
            break;
        case AST_DECLARATION_TYPE_NAME:
            fprintf(f, "type name: '%.*s'", strp(node->value));
            break;
        case AST_DECLARATION_VARIABLE:
            fprintf(f, "%s: '%.*s'", node->variable_is_const ? "const" : "var", strp(node->variable_name));
            break;
        case AST_DECLARATION_FUNCTION_PARAMETER:
            fprintf(f, "%.*s", strp(node->function_parameter_name));
            break;
        case AST_DECLARATION_FUNCTION:
            fprintf(f, "function: '%.*s'", strp(node->function_name));
            break;
        case AST_STATEMENT_BLOCK:
            fprintf(f, "block");
            break;
        case AST_STATEMENT_IF:
            fprintf(f, "if");
            break;
        case AST_STATEMENT_WHILE:
            fprintf(f, "while");
            break;
        case AST_STATEMENT_DO_WHILE:
            fprintf(f, "do while");
            break;
        case AST_STATEMENT_FOR:
            fprintf(f, "for");
            break;
        case AST_STATEMENT_SWITCH:
            fprintf(f, "switch");
            break;
        case AST_STATEMENT_BREAK:
            fprintf(f, "break");
            break;
        case AST_STATEMENT_CONTINUE:
            fprintf(f, "continue");
            break;
        case AST_STATEMENT_RETURN:
            fprintf(f, "return");
            break;
        case AST_STATEMENT_INIT:
            fprintf(f, "init: '%.*s'", strp(node->init_name));
            break;
        case AST_EXPRESSION_UNARY:
            fprintf(f, "%d", node->unary_operator);
            break;
        case AST_EXPRESSION_BINARY:
            fprintf(f, "%d", node->binary_operator);
            break;
        case AST_EXPRESSION_LITERAL_STRING:
            fprintf(f, "string: '%.*s'", strp(node->value));
            break;
        case AST_EXPRESSION_LITERAL_CHAR:
            fprintf(f, "char: '%.*s'", strp(node->value));
            break;
        case AST_EXPRESSION_LITERAL_NUMBER:
            fprintf(f, "number: '%.*s'", strp(node->value));
            break;
        case AST_EXPRESSION_IDENTIFIER:
            fprintf(f, "identifier: '%.*s'", strp(node->value));
            break;
        default:
            fprintf(f, "%s", ast_node_names[node->kind]);
            break;
    }

    if (node_type(node)) {
        fprintf(f, " (");
        fprint_type(f, node_type(node));
        fprintf(f, ")");
    }

    fprintf(f, "\"]\n");
}

static void *write_node_dot(ASTNode *node, FILE *f) {
    if (!node) return null;
    write_node_name(node, f);
    switch (node->kind) {
        case AST_NONE: break;
        case AST_ERROR: break;
        case AST_PROGRAM: {
            vector_foreach_ptr(ASTNode, declaration, node->declarations) {
                if ((*declaration)->kind != AST_DECLARATION_FUNCTION) {
                    continue;
                }
                write_node_dot(*declaration, f);
                fprintf(f, "\"%p\" -> \"%p\";\n", node, *declaration);
            }
            break;
        }
        case AST_DECLARATION_TYPE_NAME: break;
        case AST_DECLARATION_TYPE_POINTER: break;
        case AST_DECLARATION_TYPE_ARRAY: break;
        case AST_DECLARATION_TYPE_FUNCTION: break;
        case AST_DECLARATION_ALIAS: {
            void *ident = write_string_dot(node->alias_name, f);
            void *type = write_node_dot(node->alias_type, f);
            fprintf(f, "\"%p\" -> \"%p\" [label=\"alias\"]\n", node, ident);
            fprintf(f, "\"%p\" -> \"%p\" [label=\"type\"]\n", node, type);
            break;
        }
        case AST_DECLARATION_AGGREGATE: break;
        case AST_DECLARATION_VARIABLE: {
            void *type = write_node_dot(node->variable_type, f);
            void *value = write_node_dot(node->variable_initializer, f);

            if (type) fprintf(f, "\"%p\" -> \"%p\" [label=\"type\"]\n", node, type);
            if (value) fprintf(f, "\"%p\" -> \"%p\" [label=\"value\"]\n", node, value);
            break;
        }
        case AST_DECLARATION_FUNCTION_PARAMETER: {
            void *type = write_node_dot(node->function_parameter_type, f);
            fprintf(f, "\"%p\" -> \"%p\" [label=\"type\"]\n", node, type);
            break;
        }
        case AST_DECLARATION_FUNCTION: {
            vector_foreach_ptr(ASTNode, parameter, node->function_parameters) {
                void *param = write_node_dot(*parameter, f);
                if (param) {
                    fprintf(f, "\"%p\" -> \"%p\" [label=\"parameter\"]\n", node, param);
                }
            }

            void *rettype = write_node_dot(node->function_return_type, f);
            void *body = write_node_dot(node->function_body, f);

            if (rettype) fprintf(f, "\"%p\" -> \"%p\" [label=\"return type\"]\n", node, rettype);
            if (body) fprintf(f, "\"%p\" -> \"%p\" [label=\"body\"]\n", node, body);
            break;
        }
        case AST_STATEMENT_BLOCK: {
            vector_foreach_ptr(ASTNode, statement, node->statements) {
                void *stmt = write_node_dot(*statement, f);
                if (stmt) {
                    fprintf(f, "\"%p\" -> \"%p\" [label=\"\"]\n", node, stmt);
                }
            }
            break;
        }
        case AST_STATEMENT_IF: {
            void *condition = write_node_dot(node->if_condition, f);
            void *tbody = write_node_dot(node->if_true, f);
            void *fbody = write_node_dot(node->if_false, f);
            if (condition) fprintf(f, "\"%p\" -> \"%p\" [label=\"condition\"]\n", node, condition);
            if (tbody) fprintf(f, "\"%p\" -> \"%p\" [label=\"true\"]\n", node, tbody);
            if (fbody) fprintf(f, "\"%p\" -> \"%p\" [label=\"false\"]\n", node, fbody);
            break;
        }
        case AST_STATEMENT_WHILE: break;
        case AST_STATEMENT_DO_WHILE: break;
        case AST_STATEMENT_FOR: break;
        case AST_STATEMENT_SWITCH: break;
        case AST_STATEMENT_BREAK: break;
        case AST_STATEMENT_CONTINUE: break;
        case AST_STATEMENT_RETURN: {
            void *value = write_node_dot(node->parent, f);
            if (value) fprintf(f, "\"%p\" -> \"%p\" [label=\"value\"]\n", node, value);
            break;
        }
        case AST_STATEMENT_INIT: {
            void *ident = write_node_dot(node->init_type, f);
            void *value = write_node_dot(node->init_value, f);
            if (ident) fprintf(f, "\"%p\" -> \"%p\" [label=\"type\"]\n", node, ident);
            if (value) fprintf(f, "\"%p\" -> \"%p\" [label=\"value\"]\n", node, value);
            break;
        };
        case AST_STATEMENT_EXPRESSION: break;
        case AST_STATEMENT_ASSIGN: break;
        case AST_EXPRESSION_PAREN: {
            void *value = write_node_dot(node->parent, f);
            if (value) fprintf(f, "\"%p\" -> \"%p\" [label=\"value\"]\n", node, value);
            break;
        }
        case AST_EXPRESSION_UNARY: break;
        case AST_EXPRESSION_BINARY: {
            void *left = write_node_dot(node->binary_left, f);
            void *right = write_node_dot(node->binary_right, f);
            fprintf(f, "\"%p\" -> \"%p\" [label=\"left\"]\n", node, left);
            fprintf(f, "\"%p\" -> \"%p\" [label=\"right\"]\n", node, right);
            break;
        }
        case AST_EXPRESSION_TERNARY: break;
        case AST_EXPRESSION_LITERAL_NUMBER: break;
        case AST_EXPRESSION_LITERAL_CHAR: break;
        case AST_EXPRESSION_LITERAL_STRING: break;
        case AST_EXPRESSION_IDENTIFIER: break;
        case AST_EXPRESSION_CALL: break;
        case AST_EXPRESSION_FIELD: break;
        case AST_EXPRESSION_INDEX: break;
        case AST_EXPRESSION_CAST: break;
        case AST_EXPRESSION_COMPOUND: break;
        case AST_EXPRESSION_COMPOUND_FIELD: break;
        case AST_EXPRESSION_COMPOUND_FIELD_NAME: break;
        case AST_EXPRESSION_COMPOUND_FIELD_INDEX: break;
    }

    return node;
}

void write_program_dot(ASTNode *node, cstring filename) {
    FILE *f = fopen(filename, "w");
    if (f == NULL) {
        fprintf(stderr, "Could not open file %s for writing\n", filename);
        exit(1);
    }
    fprintf(f, "digraph G {\n");
    write_node_dot(node, f);
    fprintf(f, "}\n");
    fclose(f);
}