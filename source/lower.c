#include "atcc.h"
#include "ati/utils.h"

static BCType build_convert_type(BuildContext *context, Type *type) {
    assert(type);

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
            BCType *parameters = vector_create(BCType);
            vector_foreach_ptr(Type, parameter_ptr, type->function_parameters) {
                BCType parameter = build_convert_type(context, *parameter_ptr);
                vector_push(parameters, parameter);
            }

            BCType function_type = bc_type_function(result, parameters, vector_length(parameters));
            function_type->is_variadic = type->function_is_variadic;

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
            string name = type->owner->aggregate_name;
            BCType aggregate = string_table_get(&context->aggregates, name);

            if (!type->is_complete) return aggregate;
            if (!aggregate) {
                aggregate = bc_type_aggregate(context->bc, name);
                string_table_set(&context->aggregates, name, aggregate);

                BCAggregate *members = vector_create(BCAggregate);
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

                bc_type_aggregate_set_body(aggregate, members, vector_length(members));
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

static BCValue build_string_equals(BuildContext *context, BCValue left, BCValue right) {
    BCValue string_equals = string_table_get(&context->functions, str("__atcc_string_equals"));
    BCValue *string_equals_args = make_n(BCValue, 2);
    string_equals_args[0] = left;
    string_equals_args[1] = right;
    return bc_insn_call(context->function, string_equals, string_equals_args, 2);
}

static BCValue build_memset(BuildContext *context, BCValue target, BCValue value, BCValue size) {
    BCValue memset_function = string_table_get(&context->functions, str("__atcc_memset"));
    BCValue *memset_args = make_n(BCValue, 3);
    memset_args[0] = target;
    memset_args[1] = value;
    memset_args[2] = size;
    return bc_insn_call(context->function, memset_function, memset_args, 3);
}

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
    BCBlock eval_right = bc_block_make(context->function);
    BCBlock set0 = bc_block_make(context->function);
    BCBlock set1 = bc_block_make(context->function);
    BCBlock last = bc_block_make(context->function);

    BCValue c0 = bc_value_make_consti(bc_type_i32, 0);
    BCValue c1 = bc_value_make_consti(bc_type_i32, 1);

    BCValue binary_l = build_expression(context, expression->binary_left);
    BCValue comparison_l = bc_insn_ne(context->function, binary_l, c0);
    bc_insn_jump_if(context->function, comparison_l, eval_right, set0);

    bc_function_set_block(context->function, eval_right);
    BCValue binary_r = build_expression(context, expression->binary_right);
    BCValue comparison_r = bc_insn_ne(context->function, binary_r, c0);
    bc_insn_jump_if(context->function, comparison_r, set1, set0);

    bc_function_set_block(context->function, set0);
    bc_insn_jump(context->function, last);

    bc_function_set_block(context->function, set1);
    bc_insn_jump(context->function, last);

    bc_function_set_block(context->function, last);

    BCValue phi = bc_insn_phi(context->function, bc_type_i32);

    BCValue phi_values[] = {c0, c1};
    BCBlock phi_blocks[] = {set0, set1};

    bc_insn_phi_add_incoming(phi, phi_values, phi_blocks, 2);
    last->input = phi->phi_result;

    return phi->phi_result;
}

static BCValue build_expression_logical_or(BuildContext *context, ASTNode *expression) {
    BCValue c0 = bc_value_make_consti(bc_type_i32, 0);
    BCValue c1 = bc_value_make_consti(bc_type_i32, 1);

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

    BCValue phi = bc_insn_phi(context->function, bc_type_i32);

    BCValue phi_values[] = {c0, c1};
    BCBlock phi_blocks[] = {set0, set1};

    bc_insn_phi_add_incoming(phi, phi_values, phi_blocks, 2);
    last->input = phi->phi_result;

    return phi->phi_result;
}

static BCValue build_expression_binary_values(BuildContext *context, TokenKind kind, BCValue binary_l, BCValue binary_r,
                                              Type *type_l, Type *type_r) {
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
        case TOKEN_EQUAL_EQUAL: {
            // String equal check is performed with a function call to a runtime function.
            if (type_l->kind == TYPE_STRING || type_r->kind == TYPE_STRING)
                return build_string_equals(context, binary_l, binary_r);
            return bc_insn_eq(context->function, binary_l, binary_r);
        }
        case TOKEN_EXCLAMATION_EQUAL: {
            // String equal check is performed with a function call to a runtime function.
            if (type_l->kind == TYPE_STRING || type_r->kind == TYPE_STRING) {
                BCValue equals = build_string_equals(context, binary_l, binary_r);
                return bc_insn_not(context->function, equals, null);
            }

            return bc_insn_ne(context->function, binary_l, binary_r);
        }
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

    return build_expression_binary_values(context, expression->binary_operator, binary_l, binary_r,
                                          node_type(expression->binary_left),
                                          node_type(expression->binary_right));
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
    bc_insn_jump(context->function, last);
    cond_t = bc_function_get_block(context->function);

    bc_function_set_block(context->function, cond_f);
    BCValue value_f = build_expression(context, expression->ternary_false);
    bc_insn_jump(context->function, last);
    cond_f = bc_function_get_block(context->function);

    bc_function_set_block(context->function, last);

    BCType type = build_convert_type(context, node_type(expression));
    BCValue phi = bc_insn_phi(context->function, type);

    BCValue phi_values[] = {value_t, value_f};
    BCBlock phi_blocks[] = {cond_t, cond_f};

    bc_insn_phi_add_incoming(phi, phi_values, phi_blocks, 2);
    last->input = phi->phi_result;

    return phi->phi_result;
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
    Type *ntype = node_type(expression);

    BCType type = build_convert_type(context, ntype);
    BCValue compound = bc_function_define(context->function, type);
    assert(compound->type->kind == BC_TYPE_POINTER);
    BCType base_type = compound->type->base;
    if (base_type->kind == BC_TYPE_ARRAY && !base_type->is_dynamic && base_type->count) {
        // Do a memset to zero out the array.
        BCValue data_pointer = bc_insn_get_field(context->function, compound, base_type->element, 1);
        BCValue data_size = bc_insn_mul(context->function, bc_value_make_consti(bc_type_u64, base_type->element->size),
                                        base_type->count);
        build_memset(context, data_pointer, bc_value_make_consti(bc_type_u8, 0), data_size);
    } else {
        BCValue data_size = bc_value_make_consti(bc_type_u64, compound->type->size);
        build_memset(context, compound, bc_value_make_consti(bc_type_u8, 0), data_size);
    }

    u64 current_default_index = 0;
    vector_foreach_ptr(ASTNode, field_ptr, expression->compound_fields) {
        ASTNode *field = *field_ptr;
        switch (field->kind) {
            case AST_EXPRESSION_COMPOUND_FIELD: {
                BCType field_type = build_convert_type(context, node_type(field));
                BCValue value = build_expression(context, field->compound_field_target);
                BCValue target = null;

                // Strings and arrays need to access the data pointer.
                switch (ntype->kind) {
                    case TYPE_ARRAY:
                    case TYPE_STRING: {
                        BCValue index = bc_value_make_consti(bc_type_u64, current_default_index++);
                        target = bc_insn_get_field(context->function, compound, field_type, 1);

                        target = bc_insn_get_index(context->function, target, field_type, index);
                        break;
                    }
                    case TYPE_AGGREGATE: {
                        target = bc_insn_get_field(context->function, compound, field_type, current_default_index++);
                        break;
                    }
                    default: assert(!"unreachable"); break;
                }

                bc_insn_store(context->function, target, value);
                break;
            }
            case AST_EXPRESSION_COMPOUND_FIELD_NAME: {
                BCType field_type = build_convert_type(context, node_type(field));
                BCValue value = build_expression(context, field->compound_field_target);
                BCValue target = null;

                for (u64 i = 0; i < vector_length(ntype->fields); i++) {
                    TypeField *type_field = &ntype->fields[i];
                    if (string_match(type_field->name, field->compound_field_name)) {
                        target = bc_insn_get_field(context->function, compound, field_type, i);
                        break;
                    }
                }

                assert(target);

                bc_insn_store(context->function, target, value);
                break;
            }
            case AST_EXPRESSION_COMPOUND_FIELD_INDEX: {
                BCType field_type = build_convert_type(context, node_type(field));
                BCValue value = build_expression(context, field->compound_field_target);
                BCValue index = build_expression(context, field->compound_field_index);
                BCValue target = bc_insn_get_field(context->function, compound, field_type, 1);

                target = bc_insn_get_index(context->function, target, field_type, index);

                bc_insn_store(context->function, target, value);
                break;
            }
            default: assert(!"unreachable"); break;
        }
    }

    // Store the size of the array if it's statically sized.
    if (ntype->kind == TYPE_ARRAY && ntype->array_size != 0) {
        BCValue size = bc_value_make_consti(bc_type_u32, ntype->array_size);
        BCValue target = bc_insn_get_field(context->function, compound, bc_type_u32, 0);
        bc_insn_store(context->function, target, size);
    }

    return compound;
}

static BCValue build_expression(BuildContext *context, ASTNode *expression) {
    // assert(!expression->conv_type);

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
        case AST_EXPRESSION_LITERAL_STRING: return bc_insn_load(context->function, build_expression_lvalue(context, expression));
        case AST_EXPRESSION_IDENTIFIER: {
            // TODO: Constant checks, string checks, pointer checks.
            BCValue resolved = build_resolve_name(context, expression->value);
            if (resolved->kind == BC_VALUE_LOCAL || resolved->kind == BC_VALUE_GLOBAL || resolved->kind == BC_VALUE_STRING)
                resolved = bc_insn_load(context->function, resolved);
            return resolved;
        }
        case AST_EXPRESSION_SIZEOF: {
            BCType target_type = build_convert_type(context, node_type(expression->sizeof_type));
            return bc_value_make_consti(bc_type_u64, target_type->size);
        }
        case AST_EXPRESSION_ALIGNOF: assert(!"unimplemented"); break;
        case AST_EXPRESSION_OFFSETOF: assert(!"unimplemented"); break;
        case AST_EXPRESSION_CALL: {
            // TODO: Check if it's a function pointer.
            BCValue target;
            if (node_type(expression->call_target)->kind == TYPE_STRING)
                target = build_resolve_name(context, expression->call_target->literal_value);
            else
                target = build_expression(context, expression->call_target);
            BCValue *arguments = vector_create(BCValue);

            vector_foreach_ptr(ASTNode, argument_ptr, expression->call_arguments) {
                BCValue argument = build_expression(context, *argument_ptr);
                vector_push(arguments, argument);
            }

            return bc_insn_call(context->function, target, arguments, vector_length(arguments));
        }
        case AST_EXPRESSION_FIELD:
        case AST_EXPRESSION_INDEX: return bc_insn_load(context->function, build_expression_lvalue(context, expression));
        case AST_EXPRESSION_CAST: return build_expression_cast(context, expression);
        case AST_EXPRESSION_COMPOUND: return bc_insn_load(context->function, build_expression_lvalue(context, expression));
        default: assert(!"unreachable"); return null;
    }

    return bc_insn_nop(context->function);
}

static BCValue build_expression_lvalue(BuildContext *context, ASTNode *expression) {
    switch (expression->kind) {
        case AST_EXPRESSION_PAREN: return build_expression_lvalue(context, expression->parent);
        case AST_EXPRESSION_UNARY: {
            if (expression->unary_operator == TOKEN_STAR)
                return build_expression_lvalue(context, expression->unary_target);

            assert(!"unreachable");
            return null;
        }
        case AST_EXPRESSION_LITERAL_STRING: {
            BCType type = build_convert_type(context, node_type(expression));
            return bc_value_make_string(context->bc, type, expression->literal_value);
        }
        case AST_EXPRESSION_IDENTIFIER: {
            // TODO: Pointer constants can be lvalues too.
            BCValue resolved = build_resolve_name(context, expression->value);
            assert(resolved->kind != BC_VALUE_CONSTANT);
            return resolved;
        }
        // case AST_EXPRESSION_CALL: return null;
        case AST_EXPRESSION_FIELD: {
            BCValue target = build_expression_lvalue(context, expression->field_target);
            BCType type = target->type;

            assert(type->kind == BC_TYPE_POINTER);
            type = type->base;

            // Field needs a pointer by default.
            // Double pointer? Dereference.
            if (type->kind == BC_TYPE_POINTER) {
                type = type->base;
                target = bc_insn_load(context->function, target);
            }

            assert(type->kind == BC_TYPE_AGGREGATE || type->kind == BC_TYPE_ARRAY);


            if (type->kind == BC_TYPE_AGGREGATE) {
                BCType field_type = null;
                u64 field_index = 0;

                for (u32 i = 0; i < type->num_members; i++) {
                    BCAggregate *member = &type->members[i];
                    if (string_match(expression->field_name, member->name)) {
                        field_type = member->type;
                        break;
                    }
                    field_index++;
                }

                assert(field_type);
                return bc_insn_get_field(context->function, target, field_type, field_index);
            }

            if (type->kind == BC_TYPE_ARRAY) {
                BCType field_type = null;
                u64 field_index = 0;
                if (string_match(expression->field_name, str("length"))) {
                    field_index = 0;
                    field_type = bc_type_u32;
                } else if (string_match(expression->field_name, str("data"))) {
                    field_index = 1;
                    field_type = type->kind == TYPE_ARRAY
                                         ? bc_type_pointer(type->element)
                                         : bc_type_pointer(bc_type_u8);
                } else
                    assert(!"unreachable");

                return bc_insn_get_field(context->function, target, field_type, field_index);
            }
        }
#if 0
        case AST_EXPRESSION_INDEX: {
            BCValue target = build_expression(context, expression->index_target);
            BCValue index = build_expression(context, expression->index_index);

            Type *type = node_type(expression->index_target);
            assert(type->kind == TYPE_ARRAY || type->kind == TYPE_POINTER || type->kind == TYPE_STRING);

            // For arrays and strings, we need to get the data pointer.
            if (type->kind != TYPE_POINTER) {
                BCType element_type = type->kind == TYPE_ARRAY ? build_convert_type(context, type->array_base) : bc_type_u8;
                BCType field_type = bc_type_pointer(element_type);

                target = bc_insn_get_field(context->function, target, field_type, 1);

                if (type->kind == TYPE_ARRAY && type->array_size && !type->array_is_dynamic)
                    return bc_insn_get_index(context->function, target, build_convert_type(context, type->base_type), index);

                target = bc_insn_load(context->function, target);
                return bc_insn_get_index(context->function, target, element_type, index);
            } else {
                BCType element_type = build_convert_type(context, type->base_type);
                return bc_insn_get_index(context->function, target, element_type, index);
            }
        }
#endif
        case AST_EXPRESSION_INDEX: {
            BCValue index = build_expression(context, expression->index_index);

            Type *type = node_type(expression->index_target);
            assert(type->kind == TYPE_ARRAY || type->kind == TYPE_POINTER || type->kind == TYPE_STRING);

            // For arrays, we need to get the data pointer.
            if (type->kind == TYPE_ARRAY) {
                BCValue target = build_expression_lvalue(context, expression->index_target);
                BCType element_type = build_convert_type(context, type->array_base);
                BCType field_type = bc_type_pointer(element_type);

                // Static arrays are stored inline if they are sized, so we can't get the data pointer,
                // since a == &a for some reason in C.
                if (!type->array_is_dynamic && type->array_size) {
                    target = bc_insn_get_field(context->function, target, element_type, 1);
                    return bc_insn_get_index(context->function, target, build_convert_type(context, type->base_type), index);
                }

                target = bc_insn_get_field(context->function, target, field_type, 1);
                target = bc_insn_load(context->function, target);
                return bc_insn_get_index(context->function, target, element_type, index);
            }
#if false
            // For strings, we need to get the data pointer.
            else if (type->kind == TYPE_STRING) {
                BCValue target = build_expression_lvalue(context, expression->index_target);
                BCType element_type = bc_type_u8;
                BCType field_type = bc_type_pointer(element_type);

                target = bc_insn_get_field(context->function, target, field_type, 1);

                target = bc_insn_load(context->function, target);
                return bc_insn_get_index(context->function, target, element_type, index);
            }
#endif
            // For pointers, we don't need to get the data pointer.
            else {
                assert(type->kind == TYPE_POINTER);
                BCValue target = build_expression(context, expression->index_target);
                BCType element_type = build_convert_type(context, type->base_type);
                return bc_insn_get_index(context->function, target, element_type, index);
            }
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
    BCBlock last = null;

    BCValue condition = statement->if_condition ? build_expression(context, statement->if_condition)
                                                : string_table_get(&context->locals, statement->init_name);
    BCValue comparison = bc_insn_ne(context->function, condition, c0);
    bc_insn_jump_if(context->function, comparison, cond_t, cond_f);

    bc_function_set_block(context->function, cond_t);
    build_statement(context, statement->if_true);
    cond_t = bc_function_get_block(context->function);
    if (!bc_block_is_terminated(cond_t)) {
        if (!last) last = bc_block_make(context->function);
        bc_insn_jump(context->function, last);
    }

    bc_function_set_block(context->function, cond_f);
    if (statement->if_false) build_statement(context, statement->if_false);
    cond_f = bc_function_get_block(context->function);
    if (!bc_block_is_terminated(cond_f)) {
        if (!last) last = bc_block_make(context->function);
        bc_insn_jump(context->function, last);
    }

    if (last) bc_function_set_block(context->function, last);
}

static void build_statement_while(BuildContext *context, ASTNode *statement) {
    BCBlock cond = bc_block_make(context->function);
    BCBlock body = bc_block_make(context->function);
    BCBlock last = bc_block_make(context->function);

    BCBlock old_break_target = context->break_target;
    BCBlock old_continue_target = context->continue_target;

    context->break_target = last;
    context->continue_target = cond;

    bc_insn_jump(context->function, cond);

    bc_function_set_block(context->function, cond);
    BCValue c0 = bc_value_make_consti(bc_type_u32, 0);
    BCValue condition = build_expression(context, statement->while_condition);
    BCValue comparison = bc_insn_ne(context->function, condition, c0);
    bc_insn_jump_if(context->function, comparison, body, last);

    bc_function_set_block(context->function, body);

    build_statement(context, statement->while_body);

    bc_insn_jump(context->function, cond);

    bc_function_set_block(context->function, last);

    context->break_target = old_break_target;
    context->continue_target = old_continue_target;
}

static void build_statement_do_while(BuildContext *context, ASTNode *statement) {
    BCBlock cond = bc_block_make(context->function);
    BCBlock body = bc_block_make(context->function);
    BCBlock last = bc_block_make(context->function);

    bc_insn_jump(context->function, body);

    bc_function_set_block(context->function, body);

    BCBlock old_break_target = context->break_target;
    BCBlock old_continue_target = context->continue_target;

    context->break_target = last;
    context->continue_target = cond;

    build_statement(context, statement->while_body);
    body = bc_function_get_block(context->function);
    if (!bc_block_is_terminated(body))
        bc_insn_jump(context->function, cond);

    bc_function_set_block(context->function, cond);
    BCValue c0 = bc_value_make_consti(bc_type_u32, 0);
    BCValue condition = build_expression(context, statement->while_condition);
    BCValue comparison = bc_insn_ne(context->function, condition, c0);
    bc_insn_jump_if(context->function, comparison, body, last);

    bc_function_set_block(context->function, last);

    context->break_target = old_break_target;
    context->continue_target = old_continue_target;
}

static void build_statement_for(BuildContext *context, ASTNode *statement) {
    BCBlock cond = bc_block_make(context->function);
    BCBlock body = bc_block_make(context->function);
    BCBlock loop = bc_block_make(context->function);
    BCBlock last = bc_block_make(context->function);

    BCBlock old_break_target = context->break_target;
    BCBlock old_continue_target = context->continue_target;

    context->break_target = last;
    context->continue_target = loop;

    if (statement->for_initializer) build_statement(context, statement->for_initializer);

    bc_insn_jump(context->function, cond);
    bc_function_set_block(context->function, cond);
    if (statement->for_condition) {
        BCValue c0 = bc_value_make_consti(bc_type_u32, 0);
        BCValue condition = build_expression(context, statement->for_condition);
        BCValue comparison = bc_insn_ne(context->function, condition, c0);
        bc_insn_jump_if(context->function, comparison, body, last);
    } else {
        bc_insn_jump(context->function, body);
    }

    bc_function_set_block(context->function, body);

    build_statement(context, statement->for_body);

    bc_insn_jump(context->function, loop);

    bc_function_set_block(context->function, loop);
    if (statement->for_increment) build_statement(context, statement->for_increment);

    bc_insn_jump(context->function, cond);

    bc_function_set_block(context->function, last);

    context->break_target = old_break_target;
    context->continue_target = old_continue_target;
}

static void build_statement_switch(BuildContext *context, ASTNode *statement) {
    BCBlock break_target = bc_block_make(context->function);

    BCBlock old_break_target = context->break_target;
    context->break_target = break_target;

    BCValue condition = build_expression(context, statement->switch_expression);

    vector_foreach_ptr(ASTNode, switch_case_ptr, statement->switch_cases) {
        ASTNode *switch_case = *switch_case_ptr;

        BCBlock case_block = bc_block_make(context->function);
        BCBlock next_block = bc_block_make(context->function);

        BCValue comparison = null;

        vector_foreach_ptr(ASTNode, case_pattern_ptr, switch_case->switch_case_patterns) {
            ASTNode *case_pattern = *case_pattern_ptr;

            if (case_pattern->switch_pattern_end != null) {
                BCValue greater = bc_insn_ge(context->function, condition, build_expression(context, case_pattern->switch_pattern_start));
                BCValue less = bc_insn_le(context->function, condition, build_expression(context, case_pattern->switch_pattern_end));
                BCValue value = bc_insn_and(context->function, greater, less);
                comparison = comparison ? bc_insn_or(context->function, comparison, value) : value;
            } else {
                if (node_type(statement->switch_expression)->kind == TYPE_STRING) {
                    BCValue pattern = build_expression(context, case_pattern->switch_pattern_start);
                    BCValue value = build_string_equals(context, condition, pattern);
                    comparison = comparison ? bc_insn_or(context->function, comparison, value) : value;
                } else {
                    BCValue pattern = build_expression(context, case_pattern->switch_pattern_start);
                    BCValue value = bc_insn_eq(context->function, condition, pattern);
                    comparison = comparison ? bc_insn_or(context->function, comparison, value) : value;
                }
            }
        }

        if (comparison)// Default case
            bc_insn_jump_if(context->function, comparison, case_block, next_block);
        else
            bc_insn_jump(context->function, case_block);

        bc_function_set_block(context->function, case_block);
        build_statement(context, switch_case->switch_case_body);
        if (!bc_block_is_terminated(bc_function_get_block(context->function)))
            bc_insn_jump(context->function, break_target);

        bc_function_set_block(context->function, next_block);
    }

    if (!bc_block_is_terminated(bc_function_get_block(context->function)))
        bc_insn_jump(context->function, break_target);

    bc_function_set_block(context->function, break_target);
    context->break_target = old_break_target;

    BCBlock last = bc_block_make(context->function);
    bc_insn_jump(context->function, last);

    bc_function_set_block(context->function, last);
}

static void build_statement(BuildContext *context, ASTNode *statement) {
    switch (statement->kind) {
        case AST_STATEMENT_BLOCK: build_statement_block(context, statement); break;
        case AST_STATEMENT_IF: build_statement_if(context, statement); break;
        case AST_STATEMENT_WHILE: build_statement_while(context, statement); break;
        case AST_STATEMENT_DO_WHILE: build_statement_do_while(context, statement); break;
        case AST_STATEMENT_FOR: build_statement_for(context, statement); break;
        case AST_STATEMENT_SWITCH: build_statement_switch(context, statement); break;
        case AST_STATEMENT_BREAK: bc_insn_jump(context->function, context->break_target); break;
        case AST_STATEMENT_CONTINUE: bc_insn_jump(context->function, context->continue_target); break;
        case AST_STATEMENT_RETURN: bc_insn_return(context->function, build_expression(context, statement->parent)); break;
        case AST_STATEMENT_INIT: {
            BCType type = build_convert_type(context, node_type(statement));
            BCValue variable = bc_function_define(context->function, type);
            if (statement->init_value) {
                BCValue initializer = build_expression(context, statement->init_value);
                bc_insn_store(context->function, variable, initializer);
            }

            // Store the array length if we know it.
            if (type->kind == BC_TYPE_ARRAY && type->count) {
                BCValue length = bc_insn_get_field(context->function, variable, bc_type_u32, 0);
                bc_insn_store(context->function, length, type->count);
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

            BCValue result = build_expression_binary_values(context, op, lhs, rhs, node_type(statement->assign_target),
                                                            node_type(statement->assign_value));
            bc_insn_store(context->function, target, result);
            break;
        }
        default: assert(!"unreachable"); break;
    }
}

static void build_function(BuildContext *context, ASTNode *function) {
    if (string_match(function->function_name, str("__atcc_init_globals")))
        return;

    string_table_destroy(&context->locals);

    BCValue function_value = string_table_get(&context->functions, function->function_name);
    context->function = (BCFunction) function_value->storage;
    context->function->is_variadic = function->function_is_variadic;

    if (!function->function_body) {
        context->function->is_extern = true;
        return;
    }

    for (u32 i = 0; i < context->function->signature->num_params; i++) {
        ASTNode *parameter = function->function_parameters[i];
        BCValue value = bc_value_get_parameter(context->function, i);
        BCValue pointer = bc_function_define(context->function, value->type);
        bc_insn_store(context->function, pointer, value);

        string_table_set(&context->locals, parameter->function_parameter_name, pointer);
    }

    build_statement(context, function->function_body);

    // TODO: For void return types, we should build the instruction on unterminated blocks.

    if (verbose & VERBOSE_BYTECODE) {
        bc_dump_function(context->function, stderr);
        fprintf(stderr, "\n\n\n");
        fflush(stderr);
    }
}

static void build_variable(BuildContext *context, ASTNode *variable) {
    BCValue global = string_table_get(&context->globals, variable->variable_name);

    if (variable->variable_is_const)
        return;

    if (variable->variable_initializer) {
        context->function = context->initializer;

        BCValue initializer = build_expression(context, variable->variable_initializer);
        bc_insn_store(context->function, global, initializer);

        context->function = null;
    }
}

static void build_declaration(BuildContext *context, ASTNode *declaration) {
    switch (declaration->kind) {
        case AST_DECLARATION_FUNCTION: build_function(context, declaration); break;
        case AST_DECLARATION_VARIABLE: build_variable(context, declaration); break;
        default: break;
    }
}

static void build_preload_function(BuildContext *context, ASTNode *function) {
    if (string_match(function->function_name, str("__atcc_init_globals"))) {
        BCValue bc_function_value = null;
        bc_function_value = make(struct SBCValue);
        bc_function_value->kind = BC_VALUE_FUNCTION;
        bc_function_value->type = context->initializer->signature;
        bc_function_value->storage = (u64) context->initializer;

        string_table_set(&context->functions, function->function_name, bc_function_value);

        return;
    }

    Type *type = node_type(function);

    u32 num_params = vector_length(type->function_parameters);
    BCType *params = make_n(BCType, num_params);
    for (u32 i = 0; i < num_params; i++)
        params[i] = build_convert_type(context, type->function_parameters[i]);

    BCType function_type = bc_type_function(build_convert_type(context, type->function_return_type), params, num_params);
    function_type->is_variadic = function->function_is_variadic;

    BCFunction bc_function = bc_function_create(context->bc, function_type, function->function_name);
    BCValue bc_function_value = null;
    bc_function_value = make(struct SBCValue);
    bc_function_value->kind = BC_VALUE_FUNCTION;
    bc_function_value->type = function_type;
    bc_function_value->storage = (u64) bc_function;
    bc_function->is_variadic = function->function_is_variadic;

    string_table_set(&context->functions, function->function_name, bc_function_value);
}

static void build_preload_variable(BuildContext *context, ASTNode *variable) {
    Type *type = node_type(variable);
    BCType variable_type = build_convert_type(context, type);

    if (variable->variable_is_const) {
        Variant value = eval_expression(variable->variable_initializer);

        if (verbose & VERBOSE_BYTECODE) {
            printf("const %.*s = ", strp(variable->variable_name));
            variant_print(value);
            printf("\n");
        }

        if (type->kind == TYPE_STRING) {
            BCValue constant_value = bc_value_make_string(context->bc, variable_type, rawstr(value.string.data, value.string.length));
            string_table_set(&context->globals, variable->variable_name, constant_value);
            return;
        }

        BCValue constant_value = make(struct SBCValue);
        constant_value->kind = BC_VALUE_CONSTANT;
        constant_value->type = variable_type;

        switch (type->kind) {
            case TYPE_I8: constant_value->istorage = value.i8; break;
            case TYPE_U8: constant_value->storage = value.u8; break;
            case TYPE_I16: constant_value->istorage = value.i16; break;
            case TYPE_U16: constant_value->storage = value.u16; break;
            case TYPE_I32: constant_value->istorage = value.i32; break;
            case TYPE_U32: constant_value->storage = value.u32; break;
            case TYPE_I64: constant_value->istorage = value.i64; break;
            case TYPE_U64: constant_value->storage = value.u64; break;
            case TYPE_F32: constant_value->floating = value.f32; break;
            case TYPE_F64: constant_value->floating = value.f64; break;
            default: assert(!"unimplemented");
        }

        constant_value->storage = value.u64;// TODO: This is not correct for all types.

        string_table_set(&context->globals, variable->variable_name, constant_value);
    } else {
        BCValue global_value = make(struct SBCValue);
        global_value->kind = BC_VALUE_GLOBAL;
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
    // Register globals/functions, so we can refer to them e.g in a call instruction.
    vector_foreach_ptr(ASTNode, program, context->sema->programs) {
        vector_foreach_ptr(ASTNode, declaration, (*program)->declarations) {
            build_preload_declaration(context, *declaration);
        }
    }

    vector_foreach_ptr(ASTNode, program, context->sema->programs) {
        vector_foreach_ptr(ASTNode, declaration, (*program)->declarations) {
            build_declaration(context, *declaration);
        }
    }
    return true;
}
