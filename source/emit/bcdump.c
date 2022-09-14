#include <assert.h>
#include "ati/utils.h"
#include "bytecode.h"

static void bc_dump_type_base(BCType type, FILE *f) {
#define TYPE_BASE_OF(cmp, val) \
    if (type == cmp) {         \
        fprintf(f, val);       \
        return;                \
    }

    TYPE_BASE_OF(bc_type_void, "void");
    TYPE_BASE_OF(bc_type_i8, "i8");
    TYPE_BASE_OF(bc_type_u8, "u8");
    TYPE_BASE_OF(bc_type_i16, "i16");
    TYPE_BASE_OF(bc_type_u16, "u16");
    TYPE_BASE_OF(bc_type_i32, "i32");
    TYPE_BASE_OF(bc_type_u32, "u32");
    TYPE_BASE_OF(bc_type_i64, "i64");
    TYPE_BASE_OF(bc_type_u64, "u64");
    TYPE_BASE_OF(bc_type_f32, "f32");
    TYPE_BASE_OF(bc_type_f64, "f64");

    fprintf(f, "<ICE: unknown base type>");
}

static void bc_dump_type(BCType type, FILE *f) {
    switch (type->kind) {
        case BC_TYPE_BASE: bc_dump_type_base(type, f); break;
        case BC_TYPE_POINTER:
            bc_dump_type(type->base, f);
            fprintf(f, "*");
            break;
        case BC_TYPE_ARRAY: break;
        case BC_TYPE_FUNCTION:
            fprintf(f, "fun(");
            for (size_t i = 0; i < type->num_params; i++) {
                bc_dump_type(type->params[i], f);
                if (i != type->num_params - 1) fprintf(f, ", ");
            }
            fprintf(f, ")");
            if (type->result) {
                fprintf(f, ": ");
                bc_dump_type(type->result, f);
            }
            break;
        case BC_TYPE_AGGREGATE:
            fprintf(f, "%.*s", strp(type->name));
            break;
    }
}

static void bc_dump_value(BCValue value, FILE *f) {
    if (!value) return;
    if (value->flags & BC_VALUE_IS_PARAMETER) {
        fprintf(f, "param%llu[", value->storage);
        bc_dump_type(value->type, f);
        fprintf(f, "]");
        return;
    }

    if (value->flags & BC_VALUE_IS_TEMPORARY) {
        fprintf(f, "V%llu[", value->storage);
        bc_dump_type(value->type, f);
        fprintf(f, "]");
        return;
    }

    if (value->flags & BC_VALUE_IS_CONSTANT) {
        if (value->type->kind != BC_TYPE_BASE) {
            fprintf(f, "unknown_constant[");
            bc_dump_type(value->type, f);
            fprintf(f, "]");
        } else if (value->type->is_floating) {
            if (value->type == bc_type_f32)
                fprintf(f, "%f", bitcast(u32, f32, (u32) value->storage));
            else
                fprintf(f, "%f", bitcast(u64, f64, value->storage));
        } else if (value->type->is_signed) {
            if (value->type->size > 4) {
                fprintf(f, "%lld", bitcast(u64, i64, value->storage));
            } else {
                fprintf(f, "%d", bitcast(u32, i32, (u32) value->storage));
            }
        } else {
            fprintf(f, "%llx", value->storage);
        }

        return;
    }

    if (value->flags & BC_VALUE_IS_ON_STACK) {
        fprintf(f, "(stack+%llu)[", value->storage);
        bc_dump_type(value->type, f);
        fprintf(f, "]");
        return;
    }

    if (value->flags & BC_VALUE_IS_FUNCTION) {
        BCFunction function = (BCFunction) value->storage;
        fprintf(f, "%.*s", strp(function->name));
        return;
    }

    fprintf(f, "todo[");
    bc_dump_type(value->type, f);
    fprintf(f, "]");
}

static void bc_dump_code(BCCode code, FILE *f) {
#define ARITH_CASE(opcode, value)     \
    case opcode:                      \
        bc_dump_value(code->regD, f); \
        fprintf(f, " = ");            \
        bc_dump_value(code->regA, f); \
        fprintf(f, " " value " ");    \
        bc_dump_value(code->regB, f); \
        break;

    switch (code->opcode) {
        case BC_OP_NOP: fprintf(f, "nop"); break;
        case BC_OP_LOAD:
            bc_dump_value(code->regD, f);
            fprintf(f, " = load ");
            bc_dump_value(code->regA, f);
            break;
        case BC_OP_GET_FIELD:
            bc_dump_value(code->regD, f);
            fprintf(f, " = ");
            bc_dump_value(code->regA, f);
            fprintf(f, "[field %llu]", code->regB->storage);
            break;
        case BC_OP_GET_INDEX:
            bc_dump_value(code->regD, f);
            fprintf(f, " = ");
            bc_dump_value(code->regA, f);
            fprintf(f, "[");
            bc_dump_value(code->regB, f);
            fprintf(f, "]");
            break;
        case BC_OP_STORE:
            fprintf(f, "store ");
            bc_dump_value(code->regA, f);
            fprintf(f, " to ");
            bc_dump_value(code->regD, f);
            break;

            ARITH_CASE(BC_OP_ADD, "+");
            ARITH_CASE(BC_OP_SUB, "-");
            ARITH_CASE(BC_OP_MUL, "*");
            ARITH_CASE(BC_OP_DIV, "/");
            ARITH_CASE(BC_OP_MOD, "%%");
            ARITH_CASE(BC_OP_NEG, "~");
            ARITH_CASE(BC_OP_NOT, "!");
            ARITH_CASE(BC_OP_AND, "&");
            ARITH_CASE(BC_OP_OR, "|");
            ARITH_CASE(BC_OP_XOR, "^");
            ARITH_CASE(BC_OP_SHL, "<<");
            ARITH_CASE(BC_OP_SHR, ">>");
            ARITH_CASE(BC_OP_EQ, "==");
            ARITH_CASE(BC_OP_NE, "!=");
            ARITH_CASE(BC_OP_LT, "<");
            ARITH_CASE(BC_OP_GT, ">");
            ARITH_CASE(BC_OP_LE, "<=");
            ARITH_CASE(BC_OP_GE, ">=");

        case BC_OP_JUMP:
            fprintf(f, "jump to block%llu", code->bbT->serial);
            if (code->regC) {
                fprintf(f, " with ");
                bc_dump_value(code->regC, f);
            }
            break;
        case BC_OP_JUMP_IF:
            fprintf(f, "jump_if ");
            bc_dump_value(code->regA, f);
            fprintf(f, " to block%llu else block%llu", code->bbT->serial, code->bbF->serial);
            break;
        case BC_OP_CALL: {
            bc_dump_value(code->result, f);
            fprintf(f, " = ");
            bc_dump_value(code->target, f);
            fprintf(f, "(");
            for (u32 i = 0; i < code->num_args; i++) {
                bc_dump_value(code->args[i], f);
                if (i < code->num_args - 1)
                    fprintf(f, ", ");
            }
            fprintf(f, ")");
            break;
        }
        case BC_OP_RETURN:
            fprintf(f, "return ");
            bc_dump_value(code->regA, f);
            break;
        case BC_OP_CAST_BITWISE:
            bc_dump_value(code->regD, f);
            fprintf(f, " = bitwise ");
            bc_dump_value(code->regA, f);
            break;
        case BC_OP_CAST_INT_TO_PTR:
            bc_dump_value(code->regD, f);
            fprintf(f, " = to ptr ");
            bc_dump_value(code->regA, f);
            break;
        case BC_OP_CAST_PTR_TO_INT:
            bc_dump_value(code->regD, f);
            fprintf(f, " = to int ");
            bc_dump_value(code->regA, f);
            break;
        case BC_OP_CAST_INT_TRUNC:
            bc_dump_value(code->regD, f);
            fprintf(f, " = itrunc ");
            bc_dump_value(code->regA, f);
            break;
        case BC_OP_CAST_INT_ZEXT:
            bc_dump_value(code->regD, f);
            fprintf(f, " = zext ");
            bc_dump_value(code->regA, f);
            break;
        case BC_OP_CAST_INT_SEXT:
            bc_dump_value(code->regD, f);
            fprintf(f, " = sext ");
            bc_dump_value(code->regA, f);
            break;
        case BC_OP_CAST_FP_EXTEND:
            bc_dump_value(code->regD, f);
            fprintf(f, " = fpext ");
            bc_dump_value(code->regA, f);
            break;
        case BC_OP_CAST_FP_TRUNC:
            bc_dump_value(code->regD, f);
            fprintf(f, " = fptrunc ");
            bc_dump_value(code->regA, f);
            break;
        case BC_OP_CAST_FP_TO_SINT:
            bc_dump_value(code->regD, f);
            fprintf(f, " = fp to sint ");
            bc_dump_value(code->regA, f);
            break;
        case BC_OP_CAST_FP_TO_UINT:
            bc_dump_value(code->regD, f);
            fprintf(f, " = fp to uint ");
            bc_dump_value(code->regA, f);
            break;
        case BC_OP_CAST_SINT_TO_FP:
            bc_dump_value(code->regD, f);
            fprintf(f, " = sint to fp ");
            bc_dump_value(code->regA, f);
            break;
        case BC_OP_CAST_UINT_TO_FP:
            bc_dump_value(code->regD, f);
            fprintf(f, " = uint to fp ");
            bc_dump_value(code->regA, f);
            break;
        default: assert(!"unimplemented opcode"); break;
    }
#undef ARITH_CASE
}

void bc_dump_function(BCFunction function, FILE *f) {
    fprintf(f, "---- func %.*s ", (i32) function->name.length, function->name.data);
    bc_dump_type(function->signature, f);
    fprintf(f, " - stack size %u\n", function->stack_size);

    for (BCBlock block = function->first_block; block; block = block->next) {
        fprintf(f, "  ---- block%llu", block->serial);
        if (block->input) {
            fprintf(f, " (accepts ");
            bc_dump_value(block->input, f);
            fprintf(f, "):\n");
        } else {
            fprintf(f, ":\n");
        }

        vector_foreach(BCCode, code_ptr, block->code) {
            fprintf(f, "    ");
            bc_dump_code(*code_ptr, f);
            fprintf(f, "\n");
        }
    }
}