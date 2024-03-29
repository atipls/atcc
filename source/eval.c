#include "atcc.h"
#include "ati/basic.h"

static Variant eval_unary(ASTNode *node) {
    Variant target = eval_expression(node->unary_target);

    switch (node->unary_operator) {
        case TOKEN_PLUS: return target;
        case TOKEN_MINUS: return variant_i32(-target.i32);
        case TOKEN_EXCLAMATION: return variant_i32(!target.i32);
        case TOKEN_TILDE: return variant_i32(~target.i32);
        default: assert(!"unreachable"); return variant_i32(0);
    }
}

#define make_eval_binary(T)                                                      \
    static Variant eval_binary_##T(ASTNode *node) {                              \
        Variant left = eval_expression(node->binary_left);                       \
        Variant right = eval_expression(node->binary_right);                     \
                                                                                 \
        switch (node->binary_operator) {                                         \
            case TOKEN_PLUS: return variant_##T(left.T + right.T);               \
            case TOKEN_MINUS: return variant_##T(left.T - right.T);              \
            case TOKEN_STAR: return variant_##T(left.T * right.T);               \
            case TOKEN_SLASH: return variant_##T(left.T / right.T);              \
            case TOKEN_PERCENT: return variant_##T(left.T % right.T);            \
            case TOKEN_AMPERSAND: return variant_##T(left.T & right.T);          \
            case TOKEN_PIPE: return variant_##T(left.T | right.T);               \
            case TOKEN_CARET: return variant_##T(left.T ^ right.T);              \
            case TOKEN_EQUAL_EQUAL: return variant_##T(left.T == right.T);       \
            case TOKEN_EXCLAMATION_EQUAL: return variant_##T(left.T != right.T); \
            case TOKEN_LESS: return variant_##T(left.T < right.T);               \
            case TOKEN_LESS_EQUAL: return variant_##T(left.T <= right.T);        \
            case TOKEN_GREATER: return variant_##T(left.T > right.T);            \
            case TOKEN_GREATER_EQUAL: return variant_##T(left.T >= right.T);     \
            case TOKEN_LEFT_SHIFT: return variant_##T(left.T << right.T);        \
            case TOKEN_RIGHT_SHIFT: return variant_##T(left.T >> right.T);       \
                                                                                 \
            default: assert(!"unreachable"); return variant_none();              \
        }                                                                        \
    }


#define make_eval_binary_float(T)                                                \
    static Variant eval_binary_##T(ASTNode *node) {                              \
        Variant left = eval_expression(node->binary_left);                       \
        Variant right = eval_expression(node->binary_right);                     \
                                                                                 \
        switch (node->binary_operator) {                                         \
            case TOKEN_PLUS: return variant_##T(left.T + right.T);               \
            case TOKEN_MINUS: return variant_##T(left.T - right.T);              \
            case TOKEN_STAR: return variant_##T(left.T * right.T);               \
            case TOKEN_SLASH: return variant_##T(left.T / right.T);              \
            case TOKEN_EQUAL_EQUAL: return variant_i32(left.T == right.T);       \
            case TOKEN_EXCLAMATION_EQUAL: return variant_i32(left.T != right.T); \
            case TOKEN_LESS: return variant_i32(left.T < right.T);               \
            case TOKEN_LESS_EQUAL: return variant_i32(left.T <= right.T);        \
            case TOKEN_GREATER: return variant_i32(left.T > right.T);            \
            case TOKEN_GREATER_EQUAL: return variant_i32(left.T >= right.T);     \
            default: assert(!"unreachable"); return variant_none();              \
        }                                                                        \
    }

make_eval_binary(u8);
make_eval_binary(i8);
make_eval_binary(u16);
make_eval_binary(i16);
make_eval_binary(u32);
make_eval_binary(i32);
make_eval_binary(u64);
make_eval_binary(i64);
make_eval_binary_float(f32);
make_eval_binary_float(f64);

static Variant eval_binary(ASTNode *node) {
    switch (node_type(node)->kind) {
        case TYPE_U8: return eval_binary_u8(node);
        case TYPE_I8: return eval_binary_i8(node);
        case TYPE_U16: return eval_binary_u16(node);
        case TYPE_I16: return eval_binary_i16(node);
        case TYPE_U32: return eval_binary_u32(node);
        case TYPE_I32: return eval_binary_i32(node);
        case TYPE_U64: return eval_binary_u64(node);
        case TYPE_I64: return eval_binary_i64(node);
        case TYPE_F32: return eval_binary_f32(node);
        case TYPE_F64: return eval_binary_f64(node);
        default: assert(!"unreachable"); return variant_none();
    }
}

static Variant eval_ternary(ASTNode *node) {
    Variant condition = eval_expression(node->ternary_condition);
    if (condition.u8)
        return eval_expression(node->ternary_true);
    else
        return eval_expression(node->ternary_false);
}

static Variant eval_literal(ASTNode *node) {
    Type *type = node_type(node);
    switch (type->kind) {
        case TYPE_I8: return variant_i8((i8) node->literal_as_i64);
        case TYPE_U8: return variant_u8((u8) node->literal_as_u64);
        case TYPE_I16: return variant_i16((i16) node->literal_as_i64);
        case TYPE_U16: return variant_u16((u16) node->literal_as_u64);
        case TYPE_I32: return variant_i32((i32) node->literal_as_i64);
        case TYPE_U32: return variant_u32((u32) node->literal_as_u64);
        case TYPE_I64: return variant_i64((i64) node->literal_as_i64);
        case TYPE_U64: return variant_u64((u64) node->literal_as_u64);
        case TYPE_F32: return variant_f32((f32) node->literal_as_f64);
        case TYPE_F64: return variant_f64((f64) node->literal_as_f64);
        case TYPE_STRING: return variant_string(node->literal_value.data, node->literal_value.length);
        case TYPE_ARRAY:
        default: assert(!"unimplemented"); return variant_none();
    }
}

static Variant eval_cast(ASTNode *node) {
    Variant target = eval_expression(node->cast_target);
    Type *type = node_type(node);
    switch (type->kind) {
        case TYPE_I8: return variant_i8((i8) target.i64);
        case TYPE_U8: return variant_u8((u8) target.u64);
        case TYPE_I16: return variant_i16((i16) target.i64);
        case TYPE_U16: return variant_u16((u16) target.u64);
        case TYPE_I32: return variant_i32((i32) target.i64);
        case TYPE_U32: return variant_u32((u32) target.u64);
        case TYPE_I64: return variant_i64((i64) target.i64);
        case TYPE_U64: return variant_u64((u64) target.u64);
        case TYPE_F32: return variant_f32((f32) target.f64);
        case TYPE_F64: return variant_f64((f64) target.f64);
        default: assert(!"unimplemented"); return variant_none();
    }
}

Variant eval_expression(ASTNode *node) {
    switch (node->kind) {
        case AST_EXPRESSION_PAREN: return eval_expression(node->parent);
        case AST_EXPRESSION_UNARY: return eval_unary(node);
        case AST_EXPRESSION_BINARY: return eval_binary(node);
        case AST_EXPRESSION_TERNARY: return eval_ternary(node);
        case AST_EXPRESSION_LITERAL_NUMBER:
        case AST_EXPRESSION_LITERAL_CHAR:
        case AST_EXPRESSION_LITERAL_STRING: return eval_literal(node);
        case AST_EXPRESSION_IDENTIFIER: return node->constant; // Hack to make enums work.
        case AST_EXPRESSION_SIZEOF:
        case AST_EXPRESSION_ALIGNOF:
        case AST_EXPRESSION_OFFSETOF:
        case AST_EXPRESSION_CALL:
        case AST_EXPRESSION_FIELD:
        case AST_EXPRESSION_INDEX: break;
        case AST_EXPRESSION_CAST: return eval_cast(node);
        case AST_EXPRESSION_COMPOUND:
        case AST_EXPRESSION_COMPOUND_FIELD:
        case AST_EXPRESSION_COMPOUND_FIELD_NAME:
        case AST_EXPRESSION_COMPOUND_FIELD_INDEX:
        default: assert(!"unreachable");
    }

    return variant_none();
}