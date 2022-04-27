#include "atcc.h"
#include "ati/utils.h"
#include <assert.h>
#include <stdarg.h>

#define unimplemented assert(!"unimplemented")


static SemanticError make_errorf(ASTNode *node, const char *msg, ...) {
    va_list args;
    va_start(args, msg);

    string message = make_string(128);
    vsnprintf((char *) message.data, message.length, msg, args);

    fprintf(stderr, "semantic error: ");
    vfprintf(stderr, msg, args);
    fprintf(stderr, "\n");

    va_end(args);

    SemanticError err;
    err.location = node->location;
    err.description = message;
    return err;
}

static SemanticScope *make_scope(SemanticScope *parent) {
    SemanticScope *scope = make(SemanticScope);
    string_table_create(&scope->entries);
    scope->parent = parent;
    return scope;
}

static SemanticEntry *make_entry() {
    SemanticEntry *entry = make(SemanticEntry);
    entry->kind = SEMA_ENTRY_NONE;
    entry->state = SEMA_STATE_UNRESOLVED;
    entry->node = null;
    entry->type = null;
    return entry;
}

static SemanticEntry *make_builtin(Type *type) {
    SemanticEntry *entry = make_entry();
    entry->kind = SEMA_ENTRY_TYPE;
    entry->state = SEMA_STATE_RESOLVED;
    entry->type = type;
    return entry;
}

static SemanticEntry *sema_get(SemanticScope *scope, string name) {
    SemanticEntry *entry = string_table_get(&scope->entries, name);
    if (entry) return entry;
    if (scope->parent) return sema_get(scope->parent, name);
    return null;
}

static bool sema_put(SemanticScope *scope, string name, SemanticEntry *entry) {
    return string_table_set(&scope->entries, name, entry);
}

SemanticContext *sema_initialize() {
    SemanticContext *context = make(SemanticContext);
    context->global = make_scope(null);
    context->scope = context->global;

    context->type_void = make_type(TYPE_VOID, 1, 1);
    context->type_i8 = make_type(TYPE_I8, 1, 1);
    context->type_u8 = make_type(TYPE_U8, 1, 1);
    context->type_i16 = make_type(TYPE_I16, 2, 2);
    context->type_u16 = make_type(TYPE_U16, 2, 2);
    context->type_i32 = make_type(TYPE_I32, 4, 4);
    context->type_u32 = make_type(TYPE_U32, 4, 4);
    context->type_i64 = make_type(TYPE_I64, 8, 8);
    context->type_u64 = make_type(TYPE_U64, 8, 8);
    context->type_f32 = make_type(TYPE_F32, 4, 4);
    context->type_f64 = make_type(TYPE_F64, 8, 8);

    string_table_set(&context->global->entries, str("void"), make_builtin(context->type_void));
    string_table_set(&context->global->entries, str("i8"), make_builtin(context->type_i8));
    string_table_set(&context->global->entries, str("u8"), make_builtin(context->type_u8));
    string_table_set(&context->global->entries, str("i16"), make_builtin(context->type_i16));
    string_table_set(&context->global->entries, str("u16"), make_builtin(context->type_u16));
    string_table_set(&context->global->entries, str("i32"), make_builtin(context->type_i32));
    string_table_set(&context->global->entries, str("u32"), make_builtin(context->type_u32));
    string_table_set(&context->global->entries, str("i64"), make_builtin(context->type_i64));
    string_table_set(&context->global->entries, str("u64"), make_builtin(context->type_u64));
    string_table_set(&context->global->entries, str("f32"), make_builtin(context->type_f32));
    string_table_set(&context->global->entries, str("f64"), make_builtin(context->type_f64));

    // TODO: string type

    return context;
}

static bool sema_register(SemanticContext *context, SemanticEntryKind kind, string name, ASTNode *node) {
    SemanticEntry *entry = make_entry();
    entry->kind = kind;
    entry->state = SEMA_STATE_UNRESOLVED;
    entry->node = node;
    if (!string_table_set(&context->scope->entries, name, entry)) {
        vector_push(context->errors, make_errorf(node, "redefinition of '%.*s'", (i32) name.length, name.data));
        return false;
    }
    return true;
}

bool sema_register_program(SemanticContext *context, ASTNode *program) {
    bool succeeded = true;
    vector_push(context->programs, program);
    vector_foreach_ptr(ASTNode, declaration, program->declarations) {
        ASTNode *node = *declaration;
        switch (node->kind) {
            case AST_DECLARATION_ALIAS: succeeded &= sema_register(context, SEMA_ENTRY_TYPE, node->alias_name, node); break;
            case AST_DECLARATION_AGGREGATE: succeeded &= sema_register(context, SEMA_ENTRY_TYPE, node->alias_name, node); break;
            case AST_DECLARATION_VARIABLE: succeeded &= sema_register(context, SEMA_ENTRY_VARIABLE, node->variable_name, node); break;
            case AST_DECLARATION_FUNCTION: succeeded &= sema_register(context, SEMA_ENTRY_FUNCTION, node->function_name, node); break;
            default: break;
        }
    }

    return succeeded;
}

static Type *sema_resolve_type(SemanticContext *context, ASTNode *node);
static bool sema_resolve_entry(SemanticContext *context, SemanticEntry *entry);

static Type *sema_resolve_type(SemanticContext *context, ASTNode *node) {
    if (!node) return context->type_void;

    switch (node->kind) {
        case AST_DECLARATION_TYPE_NAME: {
            SemanticEntry *entry = sema_get(context->scope, node->value);
            if (entry && sema_resolve_entry(context, entry))
                return entry->type;
            vector_push(context->errors, make_errorf(node, "type name '%.*s' is not defined", (i32) node->value.length, node->value.data));
            return null;
        }
        default: unimplemented;
    }
}

static Type *sema_resolve_function_type(SemanticContext *context, ASTNode *node) {
    if (node->base_type) return node->base_type;

    Type **parameters = null;
    vector_foreach_ptr(ASTNode, parameter, node->function_parameters) {
        Type *parameter_type = sema_resolve_type(context, (*parameter)->function_parameter_type);
        if (!parameter_type) return null;
        vector_push(parameters, parameter_type);
    }

    Type *return_type = sema_resolve_type(context, node->function_return_type);
    if (!return_type) return null;

    node->base_type = make_type(TYPE_FUNCTION, POINTER_SIZE, POINTER_SIZE);
    node->base_type->function_parameters = parameters;
    node->base_type->function_return_type = return_type;
    return node->base_type;
}

static Type *sema_analyze_initializer(SemanticContext *context, ASTNode *typedecl, ASTNode *initializer);
static bool sema_resolve_entry(SemanticContext *context, SemanticEntry *entry) {
    if (entry->state == SEMA_STATE_RESOLVED) return true;
    if (entry->state == SEMA_STATE_RESOLVING) {
        vector_push(context->errors, make_errorf(entry->node, "cyclic type definition"));
        return false;
    }

    entry->state = SEMA_STATE_RESOLVING;
    switch (entry->kind) {
        case SEMA_ENTRY_NONE: break;
        case SEMA_ENTRY_TYPE: {
            if (entry->node->kind == AST_DECLARATION_ALIAS)
                entry->type = sema_resolve_type(context, entry->node->alias_type);
            else {
                vector_push(context->errors, make_errorf(entry->node, "unimplemented type"));
                entry->type = null;
                unimplemented;
            }
        }
        case SEMA_ENTRY_VARIABLE: {
            if (!entry->node->variable_is_const || 1) {
                entry->type = sema_analyze_initializer(context, entry->node->variable_type, entry->node->variable_initializer);
            } else
                unimplemented;
            break;
        }
        case SEMA_ENTRY_FUNCTION: entry->type = sema_resolve_function_type(context, entry->node); break;
    }

    entry->state = SEMA_STATE_RESOLVED;
    return true;
}

static SemanticEntry *sema_resolve_name(SemanticContext *context, string name) {
    SemanticEntry *entry = sema_get(context->scope, name);
    if (entry && sema_resolve_entry(context, entry))
        return entry;
    return null;
}

static Type *sema_analyze_expression(SemanticContext *context, ASTNode *expression);

static void sema_unify_binary_operands(SemanticContext *context, ASTNode *left, ASTNode *right) {
    if (node_type(left) == node_type(right)) return;
    if (node_type(left) == context->type_f64)
        right->conv_type = context->type_f64;
    else if (node_type(right) == context->type_f64)
        left->conv_type = context->type_f64;
    else if (node_type(left) == context->type_f32)
        right->conv_type = context->type_f32;
    else if (node_type(right) == context->type_f32)
        left->conv_type = context->type_f32;
    else {
        vector_push(context->errors, make_errorf(left, "incompatible types"));
        unimplemented;
    }
}

static Type *sema_analyze_expression_binary(SemanticContext *context, ASTNode *expression) {
    Type *left = sema_analyze_expression(context, expression->binary_left);
    Type *right = sema_analyze_expression(context, expression->binary_right);

    switch (expression->binary_operator) {
        case TOKEN_STAR:
        case TOKEN_SLASH: {
            if (!type_is_arithmetic(left)) {
                vector_push(context->errors, make_errorf(expression->binary_left, "left operand must be arithmetic"));
                return null;
            }

            if (!type_is_arithmetic(right)) {
                vector_push(context->errors, make_errorf(expression->binary_right, "right operand must be arithmetic"));
                return null;
            }
        }
        default: {
            sema_unify_binary_operands(context, expression->binary_left, expression->binary_right);
            expression->base_type = left;
            return left;
        }
    }
}

static Type *sema_analyze_expression(SemanticContext *context, ASTNode *expression) {
    switch (expression->kind) {
        case AST_EXPRESSION_PAREN: {
            Type *resolved = sema_analyze_expression(context, expression->parent);
            expression->base_type = resolved;
            return resolved;
        }
        case AST_EXPRESSION_UNARY: {
            Type *resolved = sema_analyze_expression(context, expression->unary_target);
            printf("TODO Unary\n");

            expression->base_type = resolved;
            return resolved;
        }
        case AST_EXPRESSION_BINARY: return sema_analyze_expression_binary(context, expression);
        case AST_EXPRESSION_TERNARY: {
            Type *condition = sema_analyze_expression(context, expression->ternary_condition);
            if (!type_is_scalar(condition)) {
                vector_push(context->errors, make_errorf(expression->ternary_condition, "ternary condition must be scalar"));
                return null;
            }

            Type *left = sema_analyze_expression(context, expression->ternary_true);
            Type *right = sema_analyze_expression(context, expression->ternary_false);

            if (left == right) {// TODO: Better type equality check
                expression->base_type = left;
                return left;
            }

            unimplemented;
        }
        case AST_EXPRESSION_LITERAL_NUMBER: {
            expression->base_type = context->type_i32;
            // TODO: Properly handle floats, etc.
            return context->type_i32;
        }
            // case AST_EXPRESSION_LITERAL_CHAR:
            // case AST_EXPRESSION_LITERAL_STRING:
        case AST_EXPRESSION_IDENTIFIER: {
            SemanticEntry *entry = sema_resolve_name(context, expression->value);
            if (!entry) {
                vector_push(context->errors, make_errorf(expression, "undefined symbol '%s'", expression->value));
                return null;
            }

            expression->base_type = entry->type;
            return entry->type;
        }
        // case AST_EXPRESSION_CALL:
        // case AST_EXPRESSION_FIELD:
        // case AST_EXPRESSION_INDEX:
        case AST_EXPRESSION_CAST: {
            Type *target = sema_analyze_expression(context, expression->cast_target);
            Type *resolved = sema_resolve_type(context, expression->cast_type);

            if (target != resolved) {
                printf("TODO: Cast ");
                print_type(target);
                printf(" to ");
                print_type(resolved);
                printf("\n");
            }

            expression->base_type = resolved;
            return resolved;
        }
            // case AST_EXPRESSION_COMPOUND:
            // case AST_EXPRESSION_COMPOUND_FIELD:
            // case AST_EXPRESSION_COMPOUND_FIELD_NAME:
            // case AST_EXPRESSION_COMPOUND_FIELD_INDEX:
        default: unimplemented;
    }
}

static Type *sema_analyze_initializer(SemanticContext *context, ASTNode *typedecl, ASTNode *initializer) {
    if (typedecl) {
        Type *resolved = sema_resolve_type(context, typedecl);
        if (initializer) unimplemented;

        return resolved;
    }

    if (!initializer) {
        vector_push(context->errors, make_errorf(typedecl, "Inferred initializer requires a value"));
        return null;
    }

    Type *inferred = sema_analyze_expression(context, initializer);
    printf("inferred type: ");
    print_type(inferred);
    printf("\n");

    return inferred;
}

static bool sema_analyze_statement(SemanticContext *context, ASTNode *statement);
static bool sema_analyze_statement_init(SemanticContext *context, ASTNode *statement);

static bool sema_analyze_statement_block(SemanticContext *context, ASTNode *statement) {
    SemanticScope *old_scope = context->scope;
    context->scope = make_scope(old_scope);

    bool returns = false;
    vector_foreach_ptr(ASTNode, child, statement->statements)
            returns = sema_analyze_statement(context, *child) || returns;

    context->scope = old_scope;
    return returns;
}

static bool sema_analyze_statement_if(SemanticContext *context, ASTNode *statement) {
    SemanticScope *old_scope = context->scope;
    context->scope = make_scope(old_scope);

    if (statement->if_expression && !sema_analyze_statement_init(context, statement->if_expression))
        return false;
    if (statement->if_condition && !sema_analyze_expression(context, statement->if_condition))
        return false;
    else if (statement->if_expression) {
        SemanticEntry *entry = sema_resolve_name(context, statement->if_expression->init_name);
        if (!type_is_scalar(entry->type)) {
            vector_push(context->errors, make_errorf(statement->if_expression, "expression must be scalar"));
            return false;
        }
    }

    bool returns = sema_analyze_statement_block(context, statement->if_true);
    if (statement->if_false)
        returns = sema_analyze_statement_block(context, statement->if_false) && returns;
    else
        returns = false;

    context->scope = old_scope;
    return returns;
}

static bool sema_analyze_statement_init(SemanticContext *context, ASTNode *statement) {
    Type *resolved = sema_analyze_initializer(context, statement->init_type, statement->init_value);
    if (sema_get(context->scope, statement->init_name)) {
        vector_push(context->errors, make_errorf(statement, "redefinition of %.*s", (i32) statement->init_name.length, statement->init_name.data));
        return false;
    }

    SemanticEntry *entry = make_entry();
    entry->kind = SEMA_ENTRY_TYPE;
    entry->state = SEMA_STATE_RESOLVED;
    entry->node = statement;
    entry->type = resolved;
    sema_put(context->scope, statement->init_name, entry);
    statement->base_type = resolved;

    return true;
}

static bool sema_analyze_statement_assign(SemanticContext *context, ASTNode *statement) {
    Type *left = sema_analyze_expression(context, statement->assign_target);
    Type *right = sema_analyze_expression(context, statement->assign_value);

    printf("TODO: check assignment\n");

    statement->base_type = left;
    return true;
}

static bool sema_analyze_statement(SemanticContext *context, ASTNode *statement) {
    switch (statement->kind) {
        case AST_STATEMENT_BLOCK: return sema_analyze_statement_block(context, statement);
        case AST_STATEMENT_IF: return sema_analyze_statement_if(context, statement);
        // case AST_STATEMENT_WHILE:
        // case AST_STATEMENT_DO_WHILE:
        // case AST_STATEMENT_FOR:
        // case AST_STATEMENT_SWITCH:
        // case AST_STATEMENT_BREAK:
        // case AST_STATEMENT_CONTINUE:
        case AST_STATEMENT_RETURN: {
            if (statement->parent)
                sema_analyze_expression(context, statement->parent);
            return true;
        }
        case AST_STATEMENT_INIT:
            sema_analyze_statement_init(context, statement);
            return false;
            // case AST_STATEMENT_EXPRESSION:
        case AST_STATEMENT_ASSIGN: sema_analyze_statement_assign(context, statement); return false;
        default: unimplemented;
    }
}

static bool sema_analyze_variable(SemanticContext *context, ASTNode *variable) {
    SemanticEntry *entry = sema_get(context->scope, variable->variable_name);
    printf("analyzing variable %.*s\n", (i32) variable->variable_name.length, variable->variable_name.data);
    return true;
}

static bool sema_analyze_function(SemanticContext *context, ASTNode *function) {
    Type *type = sema_resolve_function_type(context, function);
    if (!type) return false;

    SemanticScope *old_scope = context->scope;
    context->scope = make_scope(context->scope);

    bool success = true;
    vector_foreach_ptr(ASTNode, parameter_ptr, function->function_parameters) {
        ASTNode *parameter = *parameter_ptr;
        Type *parameter_type = sema_resolve_type(context, parameter->function_parameter_type);
        SemanticEntry *entry = make_entry();
        entry->kind = SEMA_ENTRY_TYPE;
        entry->state = SEMA_STATE_RESOLVED;
        entry->node = parameter;
        entry->type = parameter_type;
        success &= sema_put(context->scope, parameter->function_parameter_name, entry);
    }

    printf("analyzing function '%.*s'\n", (i32) function->function_name.length, function->function_name.data);
    printf("   return type: ");
    print_type(type->function_return_type);
    printf("\n");
    vector_foreach_ptr(Type, parameter, type->function_parameters) {
        printf("   parameter: ");
        print_type(*parameter);
        printf("\n");
    }

    bool returns = sema_analyze_statement(context, function->function_body);
    bool conforms = returns || type->function_return_type == context->type_void;
    printf("   returns: %s\n", conforms ? "true" : "false");

    context->scope = old_scope;
    return true;
}

static bool sema_analyze_declaration(SemanticContext *context, ASTNode *declaration) {
    switch (declaration->kind) {
        case AST_DECLARATION_ALIAS: return true;
        case AST_DECLARATION_AGGREGATE: vector_push(context->errors, make_errorf(declaration, "unsupported declaration")); return false;
        case AST_DECLARATION_VARIABLE: return sema_analyze_variable(context, declaration);
        case AST_DECLARATION_FUNCTION: return sema_analyze_function(context, declaration);
        default: vector_push(context->errors, make_errorf(declaration, "unsupported declaration")); return false;
    }
}

static bool sema_analyze_program(SemanticContext *context, ASTNode *program) {
    bool succeeded = true;
    vector_foreach_ptr(ASTNode, declaration, program->declarations)
            succeeded &= sema_analyze_declaration(context, *declaration);
    return succeeded;
}

bool sema_analyze(SemanticContext *context) {
    bool succeeded = true;
    vector_foreach_ptr(ASTNode, program, context->programs)
            succeeded &= sema_analyze_program(context, *program);
    return succeeded;
}
