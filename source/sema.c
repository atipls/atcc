#include "atcc.h"
#include "ati/utils.h"
#include <assert.h>
#include <stdarg.h>

#define unimplemented assert(!"unimplemented")


static SemanticError make_errorv(ASTNode *node, cstring msg, va_list args) {
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

static void sema_errorf(SemanticContext *context, ASTNode *node, cstring msg, ...) {
    va_list args;
    va_start(args, msg);

    vector_push(context->errors, make_errorv(node, msg, args));

    va_end(args);
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

    pointer_table_create(&context->array_types);
    pointer_table_create(&context->pointer_types);

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
    context->type_string = make_type(TYPE_STRING, 1, 1);

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

    context->type_string = make_type(TYPE_STRING, 12, 12);

    TypeField *length = vector_add(context->type_string->fields, 1);
    length->name = str("length");
    length->type = context->type_u32;

    TypeField *data = vector_add(context->type_string->fields, 1);
    data->name = str("data");
    data->type = make_type(TYPE_POINTER, POINTER_SIZE, POINTER_SIZE);
    data->type->base_type = context->type_u8;

    string_table_set(&context->global->entries, str("string"), make_builtin(context->type_string));

    return context;
}

static bool sema_register(SemanticContext *context, SemanticEntryKind kind, string name, ASTNode *node) {
    SemanticEntry *entry = make_entry();
    entry->kind = kind;
    entry->state = SEMA_STATE_UNRESOLVED;
    entry->node = node;
    if (!string_table_set(&context->scope->entries, name, entry)) {
        sema_errorf(context, node, "redefinition of '%.*s'", (i32) name.length, name.data);
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
            case AST_DECLARATION_AGGREGATE: {
                succeeded &= sema_register(context, SEMA_ENTRY_TYPE, node->aggregate_name, node);
                if (!succeeded)
                    break;

                SemanticEntry *entry = sema_get(context->scope, node->aggregate_name);
                entry->state = SEMA_STATE_RESOLVED;
                entry->type = make_type(TYPE_AGGREGATE, 0, 0);
                entry->type->owner = node;

                break;
            }
            case AST_DECLARATION_VARIABLE: succeeded &= sema_register(context, SEMA_ENTRY_VARIABLE, node->variable_name, node); break;
            case AST_DECLARATION_FUNCTION: succeeded &= sema_register(context, SEMA_ENTRY_FUNCTION, node->function_name, node); break;
            default: break;
        }
    }

    return succeeded;
}

static Type *sema_analyze_expression_expected(SemanticContext *context, ASTNode *expression, Type *expected);
static Type *sema_analyze_expression(SemanticContext *context, ASTNode *expression);
static Type *sema_analyze_initializer(SemanticContext *context, ASTNode *typedecl, ASTNode *initializer);


static Type *sema_resolve_type(SemanticContext *context, ASTNode *node);
static bool sema_resolve_entry(SemanticContext *context, SemanticEntry *entry);

static Type *sema_resolve_type(SemanticContext *context, ASTNode *node) {
    if (!node) return context->type_void;

    switch (node->kind) {
        case AST_DECLARATION_TYPE_NAME: {
            SemanticEntry *entry = sema_get(context->scope, node->value);
            if (entry && sema_resolve_entry(context, entry))
                return entry->type;

            sema_errorf(context, node, "type name '%.*s' is not defined", (i32) node->value.length, node->value.data);
            return null;
        }
        case AST_DECLARATION_TYPE_POINTER: {
            Type *base_type = sema_resolve_type(context, node->parent);
            Type *cached_type = pointer_table_get(&context->pointer_types, base_type);
            if (cached_type) return cached_type;

            Type *pointer = make_type(TYPE_POINTER, POINTER_SIZE, POINTER_SIZE);
            pointer->base_type = base_type;

            pointer_table_set(&context->pointer_types, base_type, pointer);
            return pointer;
        }
        case AST_DECLARATION_TYPE_ARRAY: {
            Type *base_type = sema_resolve_type(context, node->array_base);
            Type *cached_type = pointer_table_get(&context->array_types, base_type);

            if (cached_type)
                return cached_type;

            if (node->array_size) {
                sema_analyze_expression(context, node->array_size);
                assert(node->array_size->kind == AST_EXPRESSION_LITERAL_NUMBER);
                u32 size = (u32) node->array_size->literal_as_u64 * base_type->size;
                Type *array = make_type(TYPE_ARRAY, size, size);// TODO: Alignment.
                array->array_base = base_type;
                array->array_size = node->array_size->literal_as_u64;

                pointer_table_set(&context->array_types, base_type, array);
                return array;
            } else {//if (!node->array_is_dynamic) {
                printf("Creating array...\n");
                Type *array = make_type(TYPE_ARRAY, 12, 12);
                array->array_base = base_type;

                pointer_table_set(&context->array_types, base_type, array);
                return array;
            }
        }
        default: unimplemented; return null;
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
    node->base_type->function_is_variadic = node->function_is_variadic;
    return node->base_type;
}

static bool sema_resolve_entry(SemanticContext *context, SemanticEntry *entry) {
    if (entry->state == SEMA_STATE_RESOLVED) return true;
    if (entry->state == SEMA_STATE_RESOLVING) {
        sema_errorf(context, entry->node, "cyclic type definition");
        return false;
    }

    entry->state = SEMA_STATE_RESOLVING;
    switch (entry->kind) {
        case SEMA_ENTRY_NONE: break;
        case SEMA_ENTRY_TYPE: {
            // Every aggregate should be resolved as we never make an unresolved one, only incomplete ones.
            assert(entry->node->kind != AST_DECLARATION_AGGREGATE);

            if (entry->node->kind == AST_DECLARATION_ALIAS) {
                entry->type = sema_resolve_type(context, entry->node->alias_type);
                break;
            }

            sema_errorf(context, entry->node, "unimplemented type");
            entry->type = null;
            unimplemented;
            break;
        }
        case SEMA_ENTRY_VARIABLE: {
            entry->type = sema_analyze_initializer(context, entry->node->variable_type, entry->node->variable_initializer);
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


static u32 sema_resolve_aggregate_item(SemanticContext *context, ASTNode *node, TypeField **fields, bool address_only) {
    switch (node->kind) {
        case AST_DECLARATION_AGGREGATE_FIELD: {
            Type *type = sema_resolve_type(context, node->aggregate_type);
            if (!type) return 0;

            u32 total_size = 0;
            vector_foreach(string, name, node->aggregate_names) {
                TypeField *field = vector_add(*fields, 1);
                field->name = *name;
                field->type = type;
                field->address_only = address_only;
                total_size += type->size;
            }

            if (address_only)
                return type->size;

            return total_size;
        }
        case AST_DECLARATION_AGGREGATE_CHILD: {
            u32 total_size = 0;
            vector_foreach_ptr(ASTNode, child, node->aggregate_items)
                    total_size += sema_resolve_aggregate_item(context, *child, fields, node->aggregate_kind == TOKEN_KW_UNION);
            return total_size;
        }
        default: assert(!"unreachable"); return 0;
    }
}

static bool sema_complete_aggregate(SemanticContext *context, Type *aggregate) {
    assert(aggregate);

    // Strings and arrays have their special treatment, they don't need to be resolved as structs.
    if (aggregate->kind == TYPE_ARRAY || aggregate->kind == TYPE_STRING)
        return true;

    if (aggregate->kind != TYPE_AGGREGATE)
        return false;
    if (aggregate->is_complete)
        return true;

    ASTNode *node = aggregate->owner->aggregate;

    TypeField *fields = null;
    u32 total_size = 0;
    vector_foreach_ptr(ASTNode, field, node->aggregate_items)
            total_size += sema_resolve_aggregate_item(context, *field, &fields, node->aggregate_kind == TOKEN_KW_UNION);

    aggregate->size = total_size;
    aggregate->pack = total_size;// TODO: alignment

    aggregate->fields = fields;
    aggregate->is_complete = true;

    return true;
}

static bool sema_node_convert_implicit(ASTNode *node, Type *target) {
    Type *source = node_type(node);
    if (!source || !target) return false;
    if (source == target) return true;

    if (source->kind == TYPE_POINTER && target->kind == TYPE_POINTER) {
        if (source->base_type == target->base_type)
            return true;
        printf("TODO: Mismatching pointers ");
        print_type(source);
        printf(" -> ");
        print_type(target);
        printf("\n");
    }

    // Cannot downcast implicitly
    if (type_is_scalar(source) && type_is_scalar(target)) {
        if (source->size >= target->size)
            return true;
    }

    return false;
}

static bool sema_node_convert_explicit(ASTNode *node, Type *target) {
    if (sema_node_convert_implicit(node, target))
        return true;

    Type *source = node_type(node);
    if (type_is_scalar(source) && type_is_scalar(target))
        return true;

    if (source->size == target->size)
        return true;

    return false;
}

static void sema_unify_binary_operands(SemanticContext *context, ASTNode *left, ASTNode *right) {
    if (node_type(left) == node_type(right)) return;
    if (node_type(left) == context->type_f64) right->conv_type = context->type_f64;
    else if (node_type(right) == context->type_f64)
        left->conv_type = context->type_f64;
    else if (node_type(left) == context->type_f32)
        right->conv_type = context->type_f32;
    else if (node_type(right) == context->type_f32)
        left->conv_type = context->type_f32;
    else if (node_type(left) == context->type_i64)
        right->conv_type = context->type_i64;
    else if (node_type(right) == context->type_i64)
        left->conv_type = context->type_i64;
    else if (node_type(left) == context->type_i32)
        right->conv_type = context->type_i32;
    else if (node_type(right) == context->type_i32)
        left->conv_type = context->type_i32;
    else if (node_type(left) == context->type_i16)
        right->conv_type = context->type_i16;
    else if (node_type(right) == context->type_i16)
        left->conv_type = context->type_i16;
    else if (node_type(left) == context->type_i8)
        right->conv_type = context->type_i8;
    else if (node_type(right) == context->type_i8)
        left->conv_type = context->type_i8;
    else if (node_type(left) == context->type_u64)
        right->conv_type = context->type_u64;
    else if (node_type(right) == context->type_u64)
        left->conv_type = context->type_u64;
    else if (node_type(left) == context->type_u32)
        right->conv_type = context->type_u32;
    else if (node_type(right) == context->type_u32)
        left->conv_type = context->type_u32;
    else if (node_type(left) == context->type_u16)
        right->conv_type = context->type_u16;
    else if (node_type(right) == context->type_u16)
        left->conv_type = context->type_u16;
    else if (node_type(left) == context->type_u8)
        right->conv_type = context->type_u8;
    else if (node_type(right) == context->type_u8)
        left->conv_type = context->type_u8;
    else {
        sema_errorf(context, left, "incompatible types (unimplemented)");
        right->conv_type = node_type(left);
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
                sema_errorf(context, expression->binary_left, "left operand must be arithmetic");
                return null;
            }

            if (!type_is_arithmetic(right)) {
                sema_errorf(context, expression->binary_right, "right operand must be arithmetic");
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

static Type *sema_analyze_expression_compound(SemanticContext *context, ASTNode *expression, Type *expected) {
    if (!expected && !expression->compound_type) {
        sema_errorf(context, expression->compound_type, "implicit compound literals cannot be inferred");
        return null;
    }

    Type *type = expression->compound_type ? sema_resolve_type(context, expression->compound_type) : expected;

    // TODO: Check that all members are of the same type

    return type;
}

static Type *sema_analyze_expression_expected(SemanticContext *context, ASTNode *expression, Type *expected) {
    switch (expression->kind) {
        case AST_EXPRESSION_PAREN: {
            Type *resolved = sema_analyze_expression(context, expression->parent);
            expression->base_type = resolved;
            return resolved;
        }
        case AST_EXPRESSION_UNARY: {
            Type *resolved = sema_analyze_expression(context, expression->unary_target);

            if (expression->unary_operator == TOKEN_STAR) {
                if (resolved->kind != TYPE_POINTER) {
                    sema_errorf(context, expression->unary_target, "cannot dereference non-pointer type");
                    return null;
                }

                expression->base_type = resolved->base_type;
                return resolved->base_type;
            }

            if (expression->unary_operator == TOKEN_AMPERSAND) {
                // TODO: Check that the target is an lvalue
                Type *pointer = make_type(TYPE_POINTER, POINTER_SIZE, POINTER_SIZE);
                pointer->base_type = resolved;

                pointer_table_set(&context->pointer_types, resolved, pointer);
                expression->base_type = pointer;
                return pointer;
            }

            expression->base_type = resolved;
            return resolved;
        }
        case AST_EXPRESSION_BINARY: return sema_analyze_expression_binary(context, expression);
        case AST_EXPRESSION_TERNARY: {
            Type *condition = sema_analyze_expression(context, expression->ternary_condition);
            if (!type_is_scalar(condition)) {
                sema_errorf(context, expression->ternary_condition, "ternary condition must be scalar");
                return null;
            }

            Type *left = sema_analyze_expression(context, expression->ternary_true);
            Type *right = sema_analyze_expression(context, expression->ternary_false);

            if (left == right) {// TODO: Better type equality check
                expression->base_type = left;
                return left;
            }

            unimplemented;
            break;
        }
        case AST_EXPRESSION_LITERAL_NUMBER: {
            // TODO: Properly handle floats, etc.
            // TODO: Check for overflow.

            if (string_to_u64(expression->literal_value, &expression->literal_as_u64)) {
                expression->base_type = context->type_i32;
                if (expression->literal_as_u64 > 0x7FFFFFFF) expression->base_type = context->type_u32;
                if (expression->literal_as_u64 > 0xFFFFFFFF) expression->base_type = context->type_i64;
                if (expression->literal_as_u64 > 0x7FFFFFFFFFFFFFFF) expression->base_type = context->type_u64;
            } else if (string_to_f64(expression->literal_value, &expression->literal_as_f64)) {
                expression->base_type = context->type_f64;
            } else {
                unimplemented;
            }

            return expression->base_type;
        }
        case AST_EXPRESSION_LITERAL_CHAR: {
            expression->base_type = context->type_u8;
            return expression->base_type;
        }
        case AST_EXPRESSION_LITERAL_STRING: {
            expression->base_type = context->type_string;
            return expression->base_type;
        }
        case AST_EXPRESSION_IDENTIFIER: {
            SemanticEntry *entry = sema_resolve_name(context, expression->value);
            if (!entry) {
                sema_errorf(context, expression, "undefined symbol '%s.*'", strp(expression->value));
                return null;
            }

            expression->base_type = entry->type;
            return entry->type;
        }
        case AST_EXPRESSION_CALL: {
            Type *target_function_type = sema_analyze_expression(context, expression->call_target);
            bool target_is_function = target_function_type->kind == TYPE_FUNCTION ||
                                      (target_function_type->kind == TYPE_POINTER && target_function_type->base_type->kind == TYPE_FUNCTION);
            if (!target_is_function) {
                sema_errorf(context, expression->call_target, "cannot call a non-function value");
                return null;
            }

            u32 wanted_args = vector_len(target_function_type->function_parameters);
            u32 given_args = vector_len(expression->call_arguments);

            if (given_args < wanted_args) {
                sema_errorf(context, expression->call_target, "function got too few arguments. wanted %u, given %u", wanted_args, given_args);
                return null;
            }

            if (given_args > wanted_args && !target_function_type->function_is_variadic) {
                sema_errorf(context, expression->call_target, "function got too many arguments. wanted %u, given %u", wanted_args, given_args);
                return null;
            }

            for (u32 i = 0; i < given_args; i++) {
                ASTNode *argument = expression->call_arguments[i];
                sema_analyze_expression(context, argument);
                if (i >= wanted_args) break;// Don't typecheck variadic arguments.

                if (!sema_node_convert_implicit(argument, target_function_type->function_parameters[i])) {
                    // TODO: Better error message?
                    sema_errorf(context, expression->call_target, "function argument %d has the wrong type", i + 1);
                    // return null;
                }
            }

            expression->base_type = target_function_type->function_return_type;
            return target_function_type->function_return_type;
        }
        case AST_EXPRESSION_FIELD: {
            Type *field_type = sema_analyze_expression(context, expression->field_target);
            if (field_type->kind == TYPE_POINTER)
                field_type = field_type->base_type;

            if (field_type->kind != TYPE_AGGREGATE && field_type->kind != TYPE_ARRAY && field_type->kind != TYPE_STRING)
                sema_errorf(context, expression->field_target, "cannot get a field of a non-aggregate.");

            if (field_type->kind == TYPE_ARRAY) {
                if (string_match(expression->field_name, str("length"))) {
                    expression->base_type = context->type_u32;
                    return expression->base_type;
                }

                if (string_match(expression->field_name, str("data"))) {
                    expression->base_type = field_type;
                    return expression->base_type;
                }

                sema_errorf(context, expression->field_target, "array has no field named '%.*s'", strp(expression->field_name));
                return null;
            }

            if (!sema_complete_aggregate(context, field_type))
                sema_errorf(context, expression, "cannot use incomplete type here");

            vector_foreach(TypeField, field, field_type->fields) {
                if (string_match(field->name, expression->field_name)) {
                    expression->base_type = field->type;
                    return field->type;
                }
            }

            sema_errorf(context, expression, "no field named '%.*s'", strp(expression->field_name));
            return null;
        }
        case AST_EXPRESSION_INDEX: {
            Type *field_type = sema_analyze_expression(context, expression->index_target);
            if (field_type->kind != TYPE_ARRAY && field_type->kind != TYPE_POINTER && field_type->kind != TYPE_STRING) {
                sema_errorf(context, expression->index_target, "can only index arrays, strings, and pointers");
                return null;
            }

            Type *index_type = sema_analyze_expression(context, expression->index_index);
            if (!type_is_integer(index_type)) {
                sema_errorf(context, expression->index_target, "array index can only be of an integer type");
                return null;
            }

            expression->base_type = field_type->base_type;
            return field_type->base_type;
        }
        case AST_EXPRESSION_CAST: {
            Type *target = sema_analyze_expression(context, expression->cast_target);
            Type *resolved = sema_resolve_type(context, expression->cast_type);
            expression->cast_type->base_type = resolved;

            if (!sema_node_convert_explicit(expression->cast_target, resolved)) {
                printf("Cannot convert type ");
                print_type(target);
                printf(" to ");
                print_type(resolved);
                printf("\n");
            } else if (target != resolved) {
                printf("TODO: Cast\n");
            }

            expression->base_type = resolved;
            return resolved;
        }
        case AST_EXPRESSION_COMPOUND: {
            Type *resolved = sema_analyze_expression_compound(context, expression, expected);
            expression->base_type = resolved;
            return resolved;
        }
        default: unimplemented; return null;
    }

    unimplemented;
    return null;
}

static Type *sema_analyze_expression(SemanticContext *context, ASTNode *expression) {
    return sema_analyze_expression_expected(context, expression, null);
}

static Type *sema_analyze_initializer(SemanticContext *context, ASTNode *typedecl, ASTNode *initializer) {
    if (typedecl) {
        Type *resolved = sema_resolve_type(context, typedecl);
        if (initializer) {
            Type *expected = sema_analyze_expression_expected(context, initializer, resolved);
            if (!sema_node_convert_implicit(initializer, resolved)) {
                sema_errorf(context, initializer, "initializer has the wrong type");
                return null;
            }
        }

        return resolved;
    }

    if (!initializer) {
        sema_errorf(context, typedecl, "inferred initializer requires a value");
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
            sema_errorf(context, statement->if_expression, "expression must be scalar");
            return false;
        }
    }

    bool returns = sema_analyze_statement(context, statement->if_true);
    if (statement->if_false)
        returns = sema_analyze_statement(context, statement->if_false) && returns;
    else
        returns = false;

    context->scope = old_scope;
    return returns;
}

static bool sema_analyze_statement_while(SemanticContext *context, ASTNode *statement) {
    SemanticScope *old_scope = context->scope;
    context->scope = make_scope(old_scope);

    sema_analyze_expression(context, statement->while_condition);

    context->scope->can_break = true;
    context->scope->can_continue = true;

    sema_analyze_statement(context, statement->while_body);

    context->scope = old_scope;
    return false;
}

static bool sema_analyze_statement_switch_case(SemanticContext *context, ASTNode *statement, Type *expression_type, bool *has_default) {
    vector_foreach_ptr(ASTNode, switch_pattern_ptr, statement->switch_case_patterns) {
        ASTNode *switch_pattern = *switch_pattern_ptr;

        sema_analyze_expression(context, switch_pattern->switch_pattern_start);
        if (!sema_node_convert_implicit(switch_pattern->switch_pattern_start, expression_type)) {
            sema_errorf(context, switch_pattern->switch_pattern_start, "invalid type in switch case start.");
            return false;
        }

        if (switch_pattern->switch_pattern_end) {
            Type *end_type = sema_analyze_expression(context, switch_pattern->switch_pattern_end);
            if (!type_is_arithmetic(end_type)) {
                sema_errorf(context, switch_pattern->switch_pattern_end, "switch case ranges must be of an arithmetic type");
                return false;
            }

            if (!sema_node_convert_implicit(switch_pattern->switch_pattern_end, expression_type)) {
                sema_errorf(context, switch_pattern->switch_pattern_end, "invalid type in switch case end.");
                return false;
            }


            // TODO: Check that start < end
            // TODO: Evaluate constant expressions
        }
    }

    if (statement->switch_case_is_default) {
        if (*has_default) sema_errorf(context, statement, "switch statement has multiple default cases");
        *has_default = true;
    }

    return sema_analyze_statement(context, statement->switch_case_body);
}

static bool sema_analyze_statement_for(SemanticContext *context, ASTNode *statement) {
    SemanticScope *old_scope = context->scope;
    context->scope = make_scope(old_scope);

    if (statement->for_initializer) sema_analyze_statement(context, statement->for_initializer);
    if (statement->for_condition) sema_analyze_expression(context, statement->for_condition);// TODO: Check if its a valid condition.
    if (statement->for_increment) sema_analyze_statement(context, statement->for_increment);

    context->scope->can_break = true;
    context->scope->can_continue = true;

    sema_analyze_statement(context, statement->for_body);

    context->scope = old_scope;
    return false;
}

static bool sema_analyze_statement_switch(SemanticContext *context, ASTNode *statement) {
    SemanticScope *old_scope = context->scope;
    context->scope = make_scope(old_scope);

    Type *expression_type = sema_analyze_expression(context, statement->switch_expression);
    if (!type_is_integer(expression_type) && expression_type != context->type_string) {
        sema_errorf(context, statement->switch_expression, "switch expression must be integer or string");
        return false;
    }

    context->scope->can_break = true;

    bool returns = true;
    bool has_default = false;
    vector_foreach_ptr(ASTNode, switch_case, statement->switch_cases)
            returns = sema_analyze_statement_switch_case(context, *switch_case, expression_type, &has_default) && returns;

    context->scope = old_scope;
    return returns;
}

static bool sema_analyze_statement_init(SemanticContext *context, ASTNode *statement) {
    Type *resolved = sema_analyze_initializer(context, statement->init_type, statement->init_value);
    if (sema_get(context->scope, statement->init_name)) {
        sema_errorf(context, statement, "redefinition of %.*s", strp(statement->init_name));
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

    if (left != right) {
        printf("TODO: Typecheck assign ");
        print_type(left);
        printf(" = ");
        print_type(right);
        printf("\n");
    }

    if (statement->assign_operator == TOKEN_EQUAL)
        statement->base_type = right;


    printf("TODO: check assignment\n");
    return true;
}

static bool sema_analyze_statement(SemanticContext *context, ASTNode *statement) {
    switch (statement->kind) {
        case AST_STATEMENT_BLOCK: return sema_analyze_statement_block(context, statement);
        case AST_STATEMENT_IF: return sema_analyze_statement_if(context, statement);
        case AST_STATEMENT_WHILE:
        case AST_STATEMENT_DO_WHILE: return sema_analyze_statement_while(context, statement);
        case AST_STATEMENT_FOR: return sema_analyze_statement_for(context, statement);
        case AST_STATEMENT_SWITCH: return sema_analyze_statement_switch(context, statement);
        case AST_STATEMENT_BREAK:
            if (!context->scope->can_break)
                sema_errorf(context, statement, "break is not allowed here");
            return false;
        case AST_STATEMENT_CONTINUE:
            if (!context->scope->can_continue)
                sema_errorf(context, statement, "continue is not allowed here");
            return false;
        case AST_STATEMENT_RETURN: {
            if (statement->parent)
                sema_analyze_expression(context, statement->parent);
            return true;
        }
        case AST_STATEMENT_INIT: sema_analyze_statement_init(context, statement); return false;
        case AST_STATEMENT_EXPRESSION: sema_analyze_expression(context, statement->parent); return false;
        case AST_STATEMENT_ASSIGN: sema_analyze_statement_assign(context, statement); return false;
        default:
            printf("Unimplemented %d\n", statement->kind);
            unimplemented;
            return false;
    }
}

static bool sema_analyze_variable(SemanticContext *context, ASTNode *variable) {
    SemanticEntry *entry = sema_resolve_name(context, variable->variable_name);

    // TODO: Stricter typechecks.
    variable->base_type = entry->type;

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

    bool returns = function->function_body ? sema_analyze_statement(context, function->function_body) : true;
    bool conforms = returns || type->function_return_type == context->type_void;
    printf("   returns: %s\n", conforms ? "true" : "false");

    context->scope = old_scope;
    return true;
}

static bool sema_analyze_declaration(SemanticContext *context, ASTNode *declaration) {
    switch (declaration->kind) {
        case AST_DECLARATION_ALIAS: return true;
        case AST_DECLARATION_AGGREGATE: return true;
        case AST_DECLARATION_VARIABLE: return sema_analyze_variable(context, declaration);
        case AST_DECLARATION_FUNCTION: return sema_analyze_function(context, declaration);
        default: sema_errorf(context, declaration, "unsupported declaration"); return false;
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
    fflush(stdout);
    return succeeded;
}
