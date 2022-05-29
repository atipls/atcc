#include "ati/utils.h"
#include "bytecode.h"
#include <assert.h>

static void bc_generate_prelude(FILE *f) {
    fprintf(f,
            "// Generated by atcc.\n"
            "#include <stdint.h>\n"
            "typedef uint8_t u8;\n"
            "typedef uint16_t u16;\n"
            "typedef uint32_t u32;\n"
            "typedef uint64_t u64;\n"
            "typedef int8_t i8;\n"
            "typedef int16_t i16;\n"
            "typedef int32_t i32;\n"
            "typedef int64_t i64;\n"
            "typedef float f32;\n"
            "typedef double f64;\n"
            "\n\n");
}


static void bc_generate_type_base(BCType type, FILE *f) {
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

static void bc_generate_type(BCType type, FILE *f) {
    if (!type) {
        fprintf(f, "void");
        return;
    }

    switch (type->kind) {
        case BC_TYPE_BASE: bc_generate_type_base(type, f); break;
        case BC_TYPE_POINTER:
            bc_generate_type(type->subtype, f);
            fprintf(f, "*");
            break;
        case BC_TYPE_ARRAY: assert(!"unimplemented"); break;
        case BC_TYPE_FUNCTION:
            fprintf(f, "(");
            for (size_t i = 0; i < type->num_params; i++) {
                bc_generate_type(type->params[i], f);
                if (i != type->num_params - 1) fprintf(f, ", ");
            }
            fprintf(f, ")");
            if (type->subtype) {
                fprintf(f, ": ");
                bc_generate_type(type->subtype, f);
            }
            break;
        case BC_TYPE_AGGREGATE: assert(!"unimplemented"); break;
    }
}

static void bc_generate_value(BCValue value, FILE *f) {
    if (value->flags & BC_VALUE_IS_CONSTANT) {
        assert(value->type->kind == BC_TYPE_BASE);

        fprintf(f, "((");
        bc_generate_type(value->type, f);
        if (value->type->is_floating) {
            fprintf(f, ")(%g))", value->floating);
        } else {
            fprintf(f, ")(%llu))", value->storage);
        }

        return;
    }

    if (value->flags & BC_VALUE_IS_PARAMETER) {
        fprintf(f, "param%llu", value->storage);
        return;
    }

    if (value->flags & BC_VALUE_IS_TEMPORARY) {
        fprintf(f, "V%llu", value->storage);
        return;
    }

    if (value->flags & BC_VALUE_IS_ON_STACK) {
        fprintf(f, "((");
        bc_generate_type(value->type, f);
        fprintf(f, ")(&STACK[%llu]))", value->storage);
        return;
    }

    if (value->flags & BC_VALUE_IS_GLOBAL) {
        fprintf(f, "((");
        bc_generate_type(value->type, f);
        fprintf(f, ")(&GLOBAL_VARIABLES[%llu]))", value->storage);
        return;
    }
}

static void bc_generate_binary_arith(BCCode code, cstring fmt, FILE *f) {
    bc_generate_type(code->regD->type, f);
    fprintf(f, " ");
    bc_generate_value(code->regD, f);
    fprintf(f, " = ");
    bc_generate_value(code->regA, f);
    fprintf(f, "%s", fmt);
    bc_generate_value(code->regB, f);
    fprintf(f, ";");
}

static void bc_generate_code(BCCode code, FILE *f) {
    switch (code->opcode) {
        case BC_OP_NOP: fprintf(f, "; // nop"); break;
        case BC_OP_LOAD:
            bc_generate_type(code->regD->type, f);
            fprintf(f, " ");
            bc_generate_value(code->regD, f);
            fprintf(f, " = *");
            bc_generate_value(code->regA, f);
            fprintf(f, ";");
            break;
        case BC_OP_LOAD_ADDRESS: fprintf(f, "// load_address"); break;
        case BC_OP_STORE:
            fprintf(f, "*");
            bc_generate_value(code->regD, f);
            fprintf(f, " = ");
            bc_generate_value(code->regA, f);
            fprintf(f, ";");
            break;
        case BC_OP_ADD: bc_generate_binary_arith(code, " + ", f); break;
        case BC_OP_SUB: bc_generate_binary_arith(code, " - ", f); break;
        case BC_OP_MUL: bc_generate_binary_arith(code, " * ", f); break;
        case BC_OP_DIV: bc_generate_binary_arith(code, " / ", f); break;
        case BC_OP_MOD: bc_generate_binary_arith(code, " %% ", f); break;
        case BC_OP_NEG:
            bc_generate_type(code->regD->type, f);
            fprintf(f, " ");
            bc_generate_value(code->regD, f);
            fprintf(f, " = -");
            bc_generate_value(code->regA, f);
            fprintf(f, ";");
            break;
        case BC_OP_NOT:
            bc_generate_type(code->regD->type, f);
            fprintf(f, " ");
            bc_generate_value(code->regD, f);
            fprintf(f, " = !");
            bc_generate_value(code->regA, f);
            fprintf(f, ";");
            break;
        case BC_OP_AND: bc_generate_binary_arith(code, " & ", f); break;
        case BC_OP_OR: bc_generate_binary_arith(code, " | ", f); break;
        case BC_OP_XOR: bc_generate_binary_arith(code, " ^ ", f); break;
        case BC_OP_SHL: bc_generate_binary_arith(code, " << ", f); break;
        case BC_OP_SHR: bc_generate_binary_arith(code, " >> ", f); break;
        case BC_OP_EQ: bc_generate_binary_arith(code, " == ", f); break;
        case BC_OP_NE: bc_generate_binary_arith(code, " != ", f); break;
        case BC_OP_LT: bc_generate_binary_arith(code, " < ", f); break;
        case BC_OP_GT: bc_generate_binary_arith(code, " > ", f); break;
        case BC_OP_LE: bc_generate_binary_arith(code, " <= ", f); break;
        case BC_OP_GE: bc_generate_binary_arith(code, " >= ", f); break;
        case BC_OP_JUMP:
            if (code->regC) {
                bc_generate_value(code->bbT->input, f);
                fprintf(f, " = ");
                bc_generate_value(code->regC, f);
                fprintf(f, "; ");
            }
            fprintf(f, "goto __block%llu;", code->bbT->serial);
            break;
        case BC_OP_JUMP_IF:
            fprintf(f, "if (");
            bc_generate_value(code->regC, f);
            fprintf(f, ") goto __block%llu; else goto __block%llu;", code->bbT->serial, code->bbF->serial);
            break;
        case BC_OP_CALL: fprintf(f, "// call"); break;
        case BC_OP_RETURN:
            fprintf(f, "return ");
            if (code->regA) bc_generate_value(code->regA, f);
            fprintf(f, ";");
            break;
        case BC_OP_CAST_BITWISE:
            fprintf(f, " /* cast_bitwise */ ");
            break;
        case BC_OP_CAST_INT_TO_PTR:
        case BC_OP_CAST_PTR_TO_INT:
        case BC_OP_CAST_INT_TRUNC:
        case BC_OP_CAST_INT_ZEXT:
        case BC_OP_CAST_INT_SEXT:
        case BC_OP_CAST_FP_EXTEND:
        case BC_OP_CAST_FP_TRUNC:
        case BC_OP_CAST_FP_TO_SINT:
        case BC_OP_CAST_FP_TO_UINT:
        case BC_OP_CAST_SINT_TO_FP:
        case BC_OP_CAST_UINT_TO_FP:
            bc_generate_type(code->regD->type, f);
            fprintf(f, " ");
            bc_generate_value(code->regD, f);
            fprintf(f, " = (");
            bc_generate_type(code->regD->type, f);
            fprintf(f, ") ");
            bc_generate_value(code->regA, f);
            fprintf(f, ";");
            break;
        default: assert(!"unimplemented opcode"); break;
    }
}

static void bc_generate_function_type(BCFunction function, FILE *f) {
    bc_generate_type(function->signature->subtype, f);
    fprintf(f, " %.*s(", strp(function->name));
    if (function->signature->num_params == 0) {
        fprintf(f, "void)");
        return;
    }

    for (u32 i = 0; i < function->signature->num_params; i++) {
        bc_generate_type(function->signature->params[i], f);
        fprintf(f, " param%d", i);
        if (i != function->signature->num_params - 1) fprintf(f, ", ");
    }
    fprintf(f, ")");
}

static void bc_generate_function(BCFunction function, FILE *f) {
    bc_generate_function_type(function, f);
    fprintf(f, " {\n");
    if (function->stack_size > 0)
        fprintf(f, "    char STACK[%d];\n", function->stack_size);

    for (BCBlock block = function->first_block; block; block = block->next) {
        if (block->input) {
            bc_generate_type(block->input->type, f);
            fprintf(f, " ");
            bc_generate_value(block->input, f);
            fprintf(f, ";\n");
        }
    }

    for (BCBlock block = function->first_block; block; block = block->next) {
        fprintf(f, "__block%llu: ;\n", block->serial);
        vector_foreach(BCCode, code, block->code) {
            fprintf(f, "    ");
            bc_generate_code(*code, f);
            fprintf(f, "\n");
        }
    }

    fprintf(f, "}\n\n");
}

bool bc_generate_source(BCContext context, FILE *f) {
    bc_generate_prelude(f);

    // TODO: Generate forward decls.
    vector_foreach(BCFunction, function_ptr, context->functions)
            bc_generate_function(*function_ptr, f);

    return false;
}