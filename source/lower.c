#include "atcc.h"
#include "ati/utils.h"

static BCType build_convert_type(BuildContext *context, Type *type) {
    if (!type) return bc_type_i32;// bc_type_void; THIS IS A HACK

    BCType cached = pointer_table_get(&context->types, type);
    if (cached) return cached;

    static BCType bc_type_string = null;

    switch (type->kind) {
        case TYPE_VOID: return bc_type_void;
        case TYPE_I8: return bc_type_i8;
        case TYPE_U8: return bc_type_u8;
        case TYPE_I16: return bc_type_i16;
        case TYPE_U16: return bc_type_u16;
        case TYPE_I32: return bc_type_i32;
        case TYPE_U32: return bc_type_u32;
        case TYPE_I64: return bc_type_i64;
        case TYPE_U64: return bc_type_u64;
        case TYPE_F32: return bc_type_f32;
        case TYPE_F64: return bc_type_f64;
        case TYPE_POINTER: {
            BCType base_type = build_convert_type(context, type->base_type);
            BCType pointer_type = bc_type_pointer(base_type);

            pointer_table_set(&context->types, type, pointer_type);
            return pointer_type;
        }
        case TYPE_FUNCTION: {
            BCType result = build_convert_type(context, type->function_return_type);
            BCType *parameters = null;
            vector_foreach_ptr(Type, parameter_ptr, type->function_parameters) {
                BCType parameter = build_convert_type(context, *parameter_ptr);
                vector_push(parameters, parameter);
            }

            BCType function_type = bc_type_function(result, parameters, vector_len(parameters));

            pointer_table_set(&context->types, type, function_type);
            return function_type;
        }
        case TYPE_STRING: {
            if (!bc_type_string) {
                BCAggregate *members = calloc(2, sizeof(BCAggregate));

                members[0].name = str("length");
                members[0].type = bc_type_u32;

                members[1].name = str("data");
                members[1].type = bc_type_pointer(bc_type_u8);
                members[1].offset = 4;

                bc_type_string = bc_type_aggregate(context->bc, str("string"));
                bc_type_aggregate_set_body(bc_type_string, members, 2);
            }

            return bc_type_string;
        }
        case TYPE_ARRAY: {
            if (type->array_size) {
                BCType base_type = build_convert_type(context, type->array_base);
                BCValue size = bc_value_make_consti(bc_type_u32, type->array_size);
                BCType array_type = bc_type_array(context->bc, base_type, size, false);

                pointer_table_set(&context->types, type, array_type);
                return array_type;
            } else {
                BCType base_type = build_convert_type(context, type->array_base);
                BCType array_type = bc_type_array(context->bc, base_type, null, type->array_is_dynamic);

                pointer_table_set(&context->types, type, array_type);
                return array_type;
            }
        }
        case TYPE_AGGREGATE: {
            assert(type->is_complete);
            string name = type->owner->aggregate_name;
            BCType aggregate = string_table_get(&context->aggregates, name);
            if (!aggregate) {
                aggregate = bc_type_aggregate(context->bc, name);
                string_table_set(&context->aggregates, name, aggregate);

                BCAggregate *members = null;
                u32 offset = 0;
                vector_foreach(TypeField, field, type->fields) {
                    BCType target_type = build_convert_type(context, field->type);
                    string target_name = field->name;

                    BCAggregate *member = vector_add(members, 1);
                    member->type = target_type;
                    member->name = target_name;
                    member->offset = offset;

                    if (!field->address_only)
                        offset += target_type->size;
                }

                bc_type_aggregate_set_body(aggregate, members, vector_len(members));
            }
            return aggregate;
        }
        default: assert(!"TODO: type in bcgen."); return bc_type_void;
    }
}

static BCValue build_resolve_name(BuildContext *context, string name) {
    BCValue resolved = string_table_get(&context->locals, name);
    if (resolved) return resolved;
    resolved = string_table_get(&context->globals, name);
    if (resolved) return resolved;
    return string_table_get(&context->functions, name);
}

static TokenKind build_get_assignment_operator(TokenKind kind) {
    switch (kind) {
        case TOKEN_PLUS_EQUAL: return TOKEN_PLUS;
        case TOKEN_MINUS_EQUAL: return TOKEN_MINUS;
        case TOKEN_STAR_EQUAL: return TOKEN_STAR;
        case TOKEN_SLASH_EQUAL: return TOKEN_SLASH;
        case TOKEN_PERCENT_EQUAL: return TOKEN_PERCENT;
        case TOKEN_AMPERSAND_EQUAL: return TOKEN_AMPERSAND;
        case TOKEN_PIPE_EQUAL: return TOKEN_PIPE;
        case TOKEN_CARET_EQUAL: return TOKEN_CARET;
        case TOKEN_TILDE_EQUAL: return TOKEN_TILDE;
        case TOKEN_AMPERSAND_AMPERSAND_EQUAL: return TOKEN_AMPERSAND_AMPERSAND;
        case TOKEN_PIPE_PIPE_EQUAL: return TOKEN_PIPE_PIPE;
        case TOKEN_LEFT_SHIFT_EQUAL: return TOKEN_LEFT_SHIFT;
        case TOKEN_RIGHT_SHIFT_EQUAL: return TOKEN_RIGHT_SHIFT;
        default: assert(!"Invalid assignment operator."); return TOKEN_PLUS;
    }
}

static BCValue build_expression(BuildContext *context, ASTNode *expression);
static BCValue build_expression_lvalue(BuildContext *context, ASTNode *expression);
static void build_statement(BuildContext *context, ASTNode *statement);

static BCValue build_expression_unary(BuildContext *context, ASTNode *expression) {
    if (expression->unary_operator == TOKEN_AMPERSAND)
        return build_expression_lvalue(context, expression->unary_target);
    if (expression->unary_operator == TOKEN_STAR)
        return bc_insn_load(context->function, build_expression(context, expression->unary_target));

    BCValue target = build_expression(context, expression->unary_target);
    switch (expression->unary_operator) {
        case TOKEN_PLUS: return target;
        case TOKEN_MINUS: return bc_insn_sub(context->function, target, null);
        case TOKEN_EXCLAMATION: return bc_insn_not(context->function, target, null);
        case TOKEN_TILDE: return bc_insn_neg(context->function, target, null);
        default: assert(!"unreachable"); return null;
    }
}

static BCValue build_expression_logical_and(BuildContext *context, ASTNode *expression) {
    BCValue c0 = bc_value_make_consti(bc_type_u32, 0);
    BCValue c1 = bc_value_make_consti(bc_type_u32, 1);

    BCBlock eval_right = bc_block_make(context->function);
    BCBlock set0 = bc_block_make(context->function);
    BCBlock set1 = bc_block_make(context->function);
    BCBlock last = bc_block_make(context->function);

    BCValue binary_l = build_expression(context, expression->binary_left);
    BCValue comparison = bc_insn_ne(context->function, binary_l, c0);
    bc_insn_jump_if(context->function, comparison, eval_right, set0);

    bc_function_set_block(context->function, eval_right);
    BCValue binary_r = build_expression(context, expression->binary_right);
    comparison = bc_insn_ne(context->function, binary_r, c0);
    bc_insn_jump_if(context->function, comparison, set1, set0);

    bc_function_set_block(context->function, set0);
    bc_insn_jump(context->function, last)->regC = c0;

    bc_function_set_block(context->function, set1);
    bc_insn_jump(context->function, last)->regC = c1;

    bc_function_set_block(context->function, last);
    last->input = bc_value_make(context->function, bc_type_u32);

    return last->input;
}

static BCValue build_expression_logical_or(BuildContext *context, ASTNode *expression) {
    BCValue c0 = bc_value_make_consti(bc_type_u32, 0);
    BCValue c1 = bc_value_make_consti(bc_type_u32, 1);

    BCBlock eval_right = bc_block_make(context->function);
    BCBlock set0 = bc_block_make(context->function);
    BCBlock set1 = bc_block_make(context->function);
    BCBlock last = bc_block_make(context->function);

    BCValue binary_l = build_expression(context, expression->binary_left);
    BCValue comparison = bc_insn_ne(context->function, binary_l, c0);
    bc_insn_jump_if(context->function, comparison, set1, eval_right);

    bc_function_set_block(context->function, eval_right);
    BCValue binary_r = build_expression(context, expression->binary_right);
    comparison = bc_insn_ne(context->function, binary_r, c0);
    bc_insn_jump_if(context->function, comparison, set1, set0);

    bc_function_set_block(context->function, set0);
    bc_insn_jump(context->function, last)->regC = c0;

    bc_function_set_block(context->function, set1);
    bc_insn_jump(context->function, last)->regC = c1;

    bc_function_set_block(context->function, last);
    last->input = bc_value_make(context->function, bc_type_u32);

    return last->input;
}

static BCValue build_expression_binary_values(BuildContext *context, TokenKind kind, BCValue binary_l, BCValue binary_r) {
    switch (kind) {
        case TOKEN_PLUS: {
            // TODO: Pointer arithmetic
            return bc_insn_add(context->function, binary_l, binary_r);
        }
        case TOKEN_MINUS: {
            // TODO: Pointer arithmetic
            return bc_insn_sub(context->function, binary_l, binary_r);
        }
        case TOKEN_STAR: return bc_insn_mul(context->function, binary_l, binary_r);
        case TOKEN_SLASH: return bc_insn_div(context->function, binary_l, binary_r);
        case TOKEN_PERCENT: return bc_insn_mod(context->function, binary_l, binary_r);
        case TOKEN_AMPERSAND: return bc_insn_and(context->function, binary_l, binary_r);
        case TOKEN_CARET: return bc_insn_xor(context->function, binary_l, binary_r);
        case TOKEN_PIPE: return bc_insn_or(context->function, binary_l, binary_r);
        case TOKEN_EQUAL_EQUAL: return bc_insn_eq(context->function, binary_l, binary_r);
        case TOKEN_EXCLAMATION_EQUAL: return bc_insn_ne(context->function, binary_l, binary_r);
        case TOKEN_GREATER: return bc_insn_gt(context->function, binary_l, binary_r);
        case TOKEN_GREATER_EQUAL: return bc_insn_ge(context->function, binary_l, binary_r);
        case TOKEN_LESS: return bc_insn_lt(context->function, binary_l, binary_r);
        case TOKEN_LESS_EQUAL: return bc_insn_le(context->function, binary_l, binary_r);
        case TOKEN_LEFT_SHIFT: return bc_insn_shl(context->function, binary_l, binary_r);
        case TOKEN_RIGHT_SHIFT: return bc_insn_shr(context->function, binary_l, binary_r);

        default: assert(!"unreachable"); return null;
    }
}

static BCValue build_expression_binary(BuildContext *context, ASTNode *expression) {
    if (expression->binary_operator == TOKEN_AMPERSAND_AMPERSAND) return build_expression_logical_and(context, expression);
    if (expression->binary_operator == TOKEN_PIPE_PIPE) return build_expression_logical_or(context, expression);

    BCValue binary_l = build_expression(context, expression->binary_left);
    BCValue binary_r = build_expression(context, expression->binary_right);

    return build_expression_binary_values(context, expression->binary_operator, binary_l, binary_r);
}

static BCValue build_expression_ternary(BuildContext *context, ASTNode *expression) {
    BCValue c0 = bc_value_make_consti(bc_type_u32, 0);

    BCBlock cond_t = bc_block_make(context->function);
    BCBlock cond_f = bc_block_make(context->function);
    BCBlock last = bc_block_make(context->function);

    BCValue condition = build_expression(context, expression->ternary_condition);
    BCValue comparison = bc_insn_ne(context->function, condition, c0);
    bc_insn_jump_if(context->function, comparison, cond_t, cond_f);

    bc_function_set_block(context->function, cond_t);
    BCValue value_t = build_expression(context, expression->ternary_true);
    bc_insn_jump(context->function, last)->regC = value_t;

    bc_function_set_block(context->function, cond_f);
    BCValue value_f = build_expression(context, expression->ternary_true);
    bc_insn_jump(context->function, last)->regC = value_f;

    bc_function_set_block(context->function, last);

    BCType type = build_convert_type(context, node_type(expression));
    last->input = bc_value_make(context->function, type);

    return last->input;
}

static BCValue build_expression_cast(BuildContext *context, ASTNode *expression) {
    BCValue target = build_expression(context, expression->cast_target);
    BCType type_src = build_convert_type(context, node_type(expression->cast_target));
    BCType type_dst = build_convert_type(context, node_type(expression->cast_type));

    // if (bc_type_equals(type_src, type_dst) return target;

    BCOpcode cast_opcode = BC_OP_CAST_BITWISE;
    if (type_src->is_floating && type_dst->is_floating)
        cast_opcode = type_src->size > type_dst->size ? BC_OP_CAST_FP_TRUNC : BC_OP_CAST_FP_EXTEND;
    else if (type_src->is_floating && !type_dst->is_floating)
        cast_opcode = type_dst->is_signed ? BC_OP_CAST_FP_TO_SINT : BC_OP_CAST_FP_TO_UINT;
    else if (!type_src->is_floating && type_dst->is_floating)
        cast_opcode = type_dst->is_signed ? BC_OP_CAST_SINT_TO_FP : BC_OP_CAST_UINT_TO_FP;
    else if (bc_type_is_integer(type_src) && bc_type_is_integer(type_dst))
        cast_opcode = type_src->size > type_dst->size ? BC_OP_CAST_INT_TRUNC : type_dst->is_signed ? BC_OP_CAST_INT_SEXT
                                                                                                   : BC_OP_CAST_INT_ZEXT;
    else if (bc_type_is_integer(type_src) && type_dst->kind == BC_TYPE_POINTER)
        cast_opcode = BC_OP_CAST_INT_TO_PTR;
    else if (type_src->kind == BC_TYPE_POINTER && bc_type_is_integer(type_dst))
        cast_opcode = BC_OP_CAST_PTR_TO_INT;
    else
        assert(type_src->size == type_dst->size);

    return bc_insn_cast(context->function, cast_opcode, target, type_dst);
}

static BCValue build_expression_compound(BuildContext *context, ASTNode *expression) {
    BCType type = build_convert_type(context, node_type(expression));
    BCValue compound = bc_function_define(context->function, type);

    u64 current_default_index = 0;
    vector_foreach_ptr(ASTNode, field_ptr, expression->compound_fields) {
        ASTNode *field = *field_ptr;
        switch (field->kind) {
            case AST_EXPRESSION_COMPOUND_FIELD: {
                BCValue value = build_expression(context, field->compound_field_target);
                BCValue index = bc_value_make_consti(bc_type_u64, current_default_index++);
                BCValue target = bc_insn_get_index(context->function, compound, type, index);

                bc_insn_store(context->function, target, value);
                break;
            }
            case AST_EXPRESSION_COMPOUND_FIELD_NAME: {
                assert(!"unreachable");
                break;
            }
            case AST_EXPRESSION_COMPOUND_FIELD_INDEX: {
                assert(!"unreachable");
                break;
            }
            default: assert(!"unreachable"); break;
        }
    }

    return compound;
}

static BCValue build_expression(BuildContext *context, ASTNode *expression) {
    //assert(!expression->conv_type);

    BCType type = build_convert_type(context, node_type(expression));
    switch (expression->kind) {
        case AST_EXPRESSION_PAREN: return build_expression(context, expression->parent);
        case AST_EXPRESSION_UNARY: return build_expression_unary(context, expression);
        case AST_EXPRESSION_BINARY: return build_expression_binary(context, expression);
        case AST_EXPRESSION_TERNARY: return build_expression_ternary(context, expression);
        case AST_EXPRESSION_LITERAL_NUMBER: return type->is_floating
                                                           ? bc_value_make_constf(type, expression->literal_as_f64)
                                                           : bc_value_make_consti(type, expression->literal_as_u64);
        case AST_EXPRESSION_LITERAL_CHAR: return bc_value_make_consti(type, expression->literal_as_u64);
        case AST_EXPRESSION_LITERAL_STRING: {
            BCValue string = bc_value_make_consti(type, 0);
            string->string = expression->literal_value;
            return string;
        }
        case AST_EXPRESSION_IDENTIFIER: {
            // TODO: Constant checks, string checks, pointer checks.
            BCValue resolved = build_resolve_name(context, expression->value);
            if (resolved->flags & (BC_VALUE_IS_ON_STACK | BC_VALUE_IS_GLOBAL))
                resolved = bc_insn_load(context->function, resolved);
            return resolved;
        }
        case AST_EXPRESSION_CALL: {
            // TODO: Check if it's a function pointer.
            BCValue target = build_expression(context, expression->call_target);
            BCValue *args = null;

            vector_foreach_ptr(ASTNode, argument_ptr, expression->call_arguments) {
                BCValue argument = build_expression(context, *argument_ptr);
                vector_push(args, argument);
            }

            return bc_insn_call(context->function, target, args, vector_len(args));
        }
        case AST_EXPRESSION_FIELD:
        case AST_EXPRESSION_INDEX: return bc_insn_load(context->function, build_expression_lvalue(context, expression));
        case AST_EXPRESSION_CAST: return build_expression_cast(context, expression);
        case AST_EXPRESSION_COMPOUND: return bc_insn_load(context->function, build_expression_lvalue(context, expression));
        default: assert(!"unreachable"); return null;
    }
}

static BCValue build_expression_lvalue(BuildContext *context, ASTNode *expression) {
    switch (expression->kind) {
        case AST_EXPRESSION_PAREN: return build_expression_lvalue(context, expression->parent);
        // case AST_EXPRESSION_UNARY: return null;
        case AST_EXPRESSION_IDENTIFIER: {
            // TODO: Pointer constants can be lvalues too.
            BCValue resolved = build_resolve_name(context, expression->value);
            assert(!(resolved->flags & BC_VALUE_IS_CONSTANT));

            return resolved;
        }
        // case AST_EXPRESSION_CALL: return null;
        case AST_EXPRESSION_FIELD: {
            Type *type = node_type(expression->field_target);
            BCValue target = null;

            // Dereference the target if it's a pointer.
            if (type->kind == TYPE_POINTER) {
                target = build_expression(context, expression->field_target);
                type = type->base_type;
            } else if (type->kind == TYPE_AGGREGATE)
                target = build_expression_lvalue(context, expression->field_target);
            else
                target = build_expression(context, expression->field_target);

            assert(type->kind == TYPE_AGGREGATE || type->kind == TYPE_STRING || type->kind == TYPE_ARRAY);

            if (type->kind == TYPE_AGGREGATE) {
                BCType field_type = null;
                u64 field_index = 0;
                vector_foreach(TypeField, field, type->fields) {
                    if (string_match(expression->field_name, field->name)) {
                        field_type = build_convert_type(context, field->type);
                        break;
                    }
                    field_index++;
                }

                assert(field_type);
                return bc_insn_get_field(context->function, target, field_type, field_index);
            }

            if (type->kind == TYPE_STRING || type->kind == TYPE_ARRAY) {
                BCType field_type = null;
                u64 field_index = 0;
                if (string_match(expression->field_name, str("length"))) {
                    field_index = 0;
                    field_type = bc_type_u32;
                } else if (string_match(expression->field_name, str("data"))) {
                    field_index = 1;
                    field_type = type->kind == TYPE_ARRAY
                                         ? bc_type_pointer(build_convert_type(context, type->array_base))
                                         : bc_type_pointer(bc_type_u8);
                } else
                    assert(!"unreachable");

                return bc_insn_get_field(context->function, target, field_type, field_index);
            }
        }
        case AST_EXPRESSION_INDEX: {
            BCValue target = build_expression(context, expression->index_target);
            BCValue index = build_expression(context, expression->index_index);

            Type *type = node_type(expression->index_target);
            assert(type->kind == TYPE_ARRAY || type->kind == TYPE_STRING);

            BCType element_type = build_convert_type(context, type->base_type);
            return bc_insn_get_index(context->function, target, element_type, index);
        }
        case AST_EXPRESSION_COMPOUND: return build_expression_compound(context, expression);
        default: assert(!"unreachable"); return null;
    }
}

static void build_statement_block(BuildContext *context, ASTNode *block) {
    // TODO: Bytecode scoping
    vector_foreach_ptr(ASTNode, statement, block->statements)
            build_statement(context, *statement);
}

static void build_statement_if(BuildContext *context, ASTNode *statement) {
    if (statement->if_expression)
        build_statement(context, statement->if_expression);

    BCValue c0 = bc_value_make_consti(bc_type_u32, 0);

    BCBlock cond_t = bc_block_make(context->function);
    BCBlock cond_f = bc_block_make(context->function);
    BCBlock last = bc_block_make(context->function);

    BCValue condition = statement->if_condition ? build_expression(context, statement->if_condition)
                                                : string_table_get(&context->locals, statement->init_name);
    BCValue comparison = bc_insn_ne(context->function, condition, c0);
    bc_insn_jump_if(context->function, comparison, cond_t, cond_f);

    bc_function_set_block(context->function, cond_t);
    build_statement(context, statement->if_true);
    cond_t = bc_function_get_block(context->function);
    if (!bc_block_is_terminated(cond_t))
        bc_insn_jump(context->function, last);

    bc_function_set_block(context->function, cond_f);
    if (statement->if_false) build_statement(context, statement->if_false);
    cond_f = bc_function_get_block(context->function);
    if (!bc_block_is_terminated(cond_f))
        bc_insn_jump(context->function, last);

    bc_function_set_block(context->function, last);
}

static void build_statement(BuildContext *context, ASTNode *statement) {
    switch (statement->kind) {
        case AST_STATEMENT_BLOCK: build_statement_block(context, statement); break;
        case AST_STATEMENT_IF: build_statement_if(context, statement); break;
        // case AST_STATEMENT_WHILE: break;
        // case AST_STATEMENT_DO_WHILE: break;
        case AST_STATEMENT_FOR: break;
        // case AST_STATEMENT_SWITCH: break;
        // case AST_STATEMENT_BREAK: break;
        // case AST_STATEMENT_CONTINUE: break;
        case AST_STATEMENT_RETURN: bc_insn_return(context->function, build_expression(context, statement->parent)); break;
        case AST_STATEMENT_INIT: {
            BCType type = build_convert_type(context, node_type(statement));
            BCValue variable = bc_function_define(context->function, type);
            if (statement->init_value) {
                BCValue initializer = build_expression(context, statement->init_value);
                bc_insn_store(context->function, variable, initializer);
            }

            string_table_set(&context->locals, statement->init_name, variable);
            break;
        }
        case AST_STATEMENT_EXPRESSION: build_expression(context, statement->parent); break;
        case AST_STATEMENT_ASSIGN: {
            BCValue target = build_expression_lvalue(context, statement->assign_target);
            if (statement->assign_operator == TOKEN_EQUAL) {
                BCValue value = build_expression(context, statement->assign_value);
                bc_insn_store(context->function, target, value);
                break;
            }

            TokenKind op = build_get_assignment_operator(statement->assign_operator);

            BCValue lhs = bc_insn_load(context->function, target);
            BCValue rhs = build_expression(context, statement->assign_value);

            BCValue result = build_expression_binary_values(context, op, lhs, rhs);
            bc_insn_store(context->function, target, result);
            break;
        }
        default: assert(!"unreachable"); break;
    }
}

static void build_function(BuildContext *context, ASTNode *function) {
    string_table_destroy(&context->locals);

    BCValue function_value = string_table_get(&context->functions, function->function_name);
    context->function = (BCFunction) function_value->storage;

    if (!function->function_body) {
        context->function->is_extern = true;
        return;
    }

    for (u32 i = 0; i < context->function->signature->num_params; i++) {
        // TODO: Should function parameters be lvalues?

        ASTNode *parameter = function->function_parameters[i];
        BCValue value = bc_value_get_parameter(context->function, i);
        string_table_set(&context->locals, parameter->function_parameter_name, value);
    }

    build_statement(context, function->function_body);
    // TODO: For void return types, we should build the instruction on unterminated blocks.

    bc_dump_function(context->function, stderr);
    fflush(stderr);
}

static void build_variable(BuildContext *context, ASTNode *variable) {
}

static void build_declaration(BuildContext *context, ASTNode *declaration) {
    switch (declaration->kind) {
        case AST_DECLARATION_FUNCTION: build_function(context, declaration); break;
        case AST_DECLARATION_VARIABLE: build_variable(context, declaration); break;
        default: break;
    }
}

static void build_preload_function(BuildContext *context, ASTNode *function) {
    Type *type = node_type(function);

    u32 num_params = vector_len(type->function_parameters);
    BCType *params = make_n(BCType, num_params);
    for (u32 i = 0; i < num_params; i++)
        params[i] = build_convert_type(context, type->function_parameters[i]);

    BCType function_type = bc_type_function(build_convert_type(context, type->function_return_type), params, num_params);
    BCFunction bc_function = bc_function_create(context->bc, function_type, function->function_name);
    BCValue bc_function_value = null;
    bc_function_value = make(struct SBCValue);
    bc_function_value->flags = BC_VALUE_IS_FUNCTION;
    bc_function_value->type = function_type;
    bc_function_value->storage = (u64) bc_function;

    string_table_set(&context->functions, function->function_name, bc_function_value);
}

static void build_preload_variable(BuildContext *context, ASTNode *variable) {
    BCType variable_type = build_convert_type(context, node_type(variable));
    if (variable->variable_is_const) {

        // HACK HACK: Hardcoded true and false, lol.
        BCValue constant_value = null;
        if (string_match(variable->variable_name, str("true"))) {
            constant_value = make(struct SBCValue);
            constant_value->flags = BC_VALUE_IS_CONSTANT;
            constant_value->type = variable_type;
            constant_value->storage = 1;
        }
        if (string_match(variable->variable_name, str("false"))) {
            constant_value = make(struct SBCValue);
            constant_value->flags = BC_VALUE_IS_CONSTANT;
            constant_value->type = variable_type;
            constant_value->storage = 0;
        }

        string_table_set(&context->globals, variable->variable_name, constant_value);
    } else {
        BCValue global_value = make(struct SBCValue);
        global_value->flags = BC_VALUE_IS_GLOBAL;
        global_value->type = bc_type_pointer(variable_type);
        global_value->storage = context->bc->global_size;
        context->bc->global_size += variable_type->size;

        string_table_set(&context->globals, variable->variable_name, global_value);
    }
}

static void build_preload_declaration(BuildContext *context, ASTNode *declaration) {
    switch (declaration->kind) {
        case AST_DECLARATION_FUNCTION: build_preload_function(context, declaration); break;
        case AST_DECLARATION_VARIABLE: build_preload_variable(context, declaration); break;
        default: break;
    }
}

BuildContext *build_initialize(SemanticContext *sema) {
    BuildContext *context = make(BuildContext);
    context->sema = sema;
    context->bc = bc_context_initialize();
    string_table_create(&context->aggregates);
    string_table_create(&context->functions);
    string_table_create(&context->globals);
    string_table_create(&context->locals);
    pointer_table_create(&context->types);

    BCType initializer_type = bc_type_function(bc_type_void, null, 0);
    context->initializer = bc_function_create(context->bc, initializer_type, str("__atcc_init_globals"));

    return context;
}

bool build_bytecode(BuildContext *context) {
    // Register globals/functions so we can refer to them e.g in a call instruction.
    vector_foreach_ptr(ASTNode, program, context->sema->programs) {
        vector_foreach_ptr(ASTNode, declaration, (*program)->declarations)
                build_preload_declaration(context, *declaration);
    }

    vector_foreach_ptr(ASTNode, program, context->sema->programs) {
        vector_foreach_ptr(ASTNode, declaration, (*program)->declarations)
                build_declaration(context, *declaration);
    }
    return true;
}
