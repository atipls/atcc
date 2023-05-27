#include "ati/utils.h"
#include "bytecode.h"
#include <assert.h>
#include <ctype.h>

static bool is_valid_c_identifier(u8 c, u32 index) {
    return index == 0 ? isalpha(c) || c == '_' : isalnum(c) || c == '_' || c == '$';
}

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

    TYPE_BASE_OF(bc_type_void, "void")
    TYPE_BASE_OF(bc_type_i8, "i8")
    TYPE_BASE_OF(bc_type_u8, "u8")
    TYPE_BASE_OF(bc_type_i16, "i16")
    TYPE_BASE_OF(bc_type_u16, "u16")
    TYPE_BASE_OF(bc_type_i32, "i32")
    TYPE_BASE_OF(bc_type_u32, "u32")
    TYPE_BASE_OF(bc_type_i64, "i64")
    TYPE_BASE_OF(bc_type_u64, "u64")
    TYPE_BASE_OF(bc_type_f32, "f32")
    TYPE_BASE_OF(bc_type_f64, "f64")

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
            bc_generate_type(type->base, f);
            fprintf(f, "*");
            break;
        case BC_TYPE_ARRAY: fprintf(f, "Array_%u", type->emit_index); break;
        case BC_TYPE_FUNCTION: fprintf(f, "Function_%u", type->emit_index); break;
        case BC_TYPE_AGGREGATE: fprintf(f, "%.*s", strp(type->name)); break;
    }
}

static void bc_generate_base_value(BCValue value, FILE *f) {
    if (value->type == bc_type_f32 || value->type == bc_type_f64) {
        fprintf(f, "%g", value->floating);
        return;
    }

    if (value->type->is_signed) {
        if (value->type->size < 8)
            fprintf(f, "%d", (i32) value->istorage);
        else
            fprintf(f, "%lld", value->istorage);
    } else {
        if (value->type->size < 8)
            fprintf(f, "%u", (u32) value->istorage);
        else
            fprintf(f, "%llu", value->istorage);
    }
}

static void bc_generate_value(BCValue value, FILE *f) {
    switch (value->kind) {
        case BC_VALUE_CONSTANT:
            assert(value->type->kind == BC_TYPE_BASE);
            fprintf(f, "((");
            bc_generate_type(value->type, f);
            fprintf(f, ")(");
            bc_generate_base_value(value, f);
            fprintf(f, "))");
            break;
        case BC_VALUE_STRING:
            fprintf(f, "(&__string_%llu)", value->string_index);
            break;
        case BC_VALUE_PARAMETER:
            fprintf(f, "param%llu", value->storage);
            break;
        case BC_VALUE_TEMPORARY:
            fprintf(f, "V%llu", value->storage);
            break;
        case BC_VALUE_PHI:
            fprintf(f, "V%llu", value->phi_result->storage);
            break;
        case BC_VALUE_LOCAL:
            fprintf(f, "(&L%llu)", value->storage);
            break;
        case BC_VALUE_GLOBAL:
            fprintf(f, "((");
            bc_generate_type(value->type, f);
            fprintf(f, ")(&GLOBAL_VARIABLES[%llu]))", value->storage);
            break;
        case BC_VALUE_FUNCTION: {
            BCFunction function = (BCFunction) value->storage;
            for (u32 i = 0; i < function->name.length; i++) {
                u8 character = function->name.data[i];
                if (character == 0) continue;
                cstring format = is_valid_c_identifier(function->name.data[i], i) ? "%c" : "$%X";
                fprintf(f, format, character);
            }
            break;
        }
    }
}

static void bc_generate_binary_arith(BCCode code, cstring fmt, FILE *f) {
    bc_generate_type(code->regD->type, f);
    fprintf(f, " ");
    bc_generate_value(code->regD, f);
    fprintf(f, " = ");
    if (code->regB) {
        bc_generate_value(code->regA, f);
        fprintf(f, "%s", fmt);
        bc_generate_value(code->regB, f);
    } else {
        fprintf(f, "%s", fmt);
        bc_generate_value(code->regA, f);
    }
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
        case BC_OP_GET_INDEX:
            bc_generate_type(code->regD->type, f);
            fprintf(f, " ");
            bc_generate_value(code->regD, f);
            fprintf(f, " = &((");
            bc_generate_value(code->regA, f);
            fprintf(f, ")[");
            bc_generate_value(code->regB, f);
            fprintf(f, "]);");
            break;
        case BC_OP_GET_FIELD:
            bc_generate_type(code->regD->type, f);
            fprintf(f, " ");
            bc_generate_value(code->regD, f);
            fprintf(f, " = &(");
            bc_generate_value(code->regA, f);
            {
                BCType type = code->regA->type;
                if (type->kind == BC_TYPE_POINTER) {
                    type = type->base;
                    fprintf(f, "->");
                } else {
                    assert(false);
                    fprintf(f, ".");
                }

                if (type->kind == BC_TYPE_ARRAY)
                    fprintf(f, "%s", code->regB->storage == 0 ? "length" : "data");
                else
                    fprintf(f, "%.*s", strp(type->members[code->regB->storage].name));

                fprintf(f, ");");
            }

            break;
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
        case BC_OP_MOD: bc_generate_binary_arith(code, " % ", f); break;
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
        case BC_OP_PHI:
            fprintf(f, "// Phi is resolved here.");
            break;
        case BC_OP_CALL: {
            if (code->result->type != bc_type_void) {
                bc_generate_type(code->result->type, f);
                fprintf(f, " ");
                bc_generate_value(code->result, f);
                fprintf(f, " = ");
            }

            bc_generate_value(code->target, f);
            fprintf(f, "(");
            for (u32 i = 0; i < code->num_args; i++) {
                bc_generate_value(code->args[i], f);
                if (i < code->num_args - 1)
                    fprintf(f, ", ");
            }
            fprintf(f, ");");
            break;
        }
        case BC_OP_RETURN:
            fprintf(f, "return ");
            if (code->regA) bc_generate_value(code->regA, f);
            fprintf(f, ";");
            break;
        case BC_OP_CAST_BITWISE:
            fprintf(f, " /* cast_bitwise */ ");
            //break;
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
    bc_generate_type(function->signature->result, f);
    fprintf(f, " ");
    for (u32 i = 0; i < function->name.length; i++) {
        u8 character = function->name.data[i];
        if (character == 0) continue;
        cstring format = is_valid_c_identifier(function->name.data[i], i) ? "%c" : "$%X";
        fprintf(f, format, character);
    }
    fprintf(f, "(");
    if (function->signature->num_params == 0) {
        fprintf(f, "void)");
        return;
    }

    for (u32 i = 0; i < function->signature->num_params; i++) {
        bc_generate_type(function->signature->params[i], f);
        fprintf(f, " param%d", i);
        if (i != function->signature->num_params - 1) fprintf(f, ", ");
    }

    if (function->is_variadic) fprintf(f, ", ...");

    fprintf(f, ")");
}

static void bc_generate_function(BCFunction function, FILE *f) {
    bc_generate_function_type(function, f);
    fprintf(f, " {\n");
    if (function->stack_size > 0) {
        fprintf(f, "    // char STACK[%d];\n", function->stack_size);
        vector_foreach(BCValue, local_ptr, function->locals) {
            BCValue local = *local_ptr;
            fprintf(f, "    ");
            bc_generate_type(local->type->base, f);
            fprintf(f, " L%llu;\n", local->storage);
        }
    }

    for (BCBlock block = function->first_block; block; block = block->next) {
        if (block->input) {
            fprintf(f, "    ");
            bc_generate_type(block->input->type, f);
            fprintf(f, " ");
            bc_generate_value(block->input, f);
            fprintf(f, "; // Phi value.\n");
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

static void bc_generate_aggregate(BCType aggregate, FILE *f) {
    fprintf(f, "struct %.*s {\n", strp(aggregate->name));
    for (u32 i = 0; i < aggregate->num_members; i++) {
        fprintf(f, "    /* 0x%04X */", aggregate->members[i].offset);
        bc_generate_type(aggregate->members[i].type, f);
        fprintf(f, " %.*s;\n", strp(aggregate->members[i].name));
    }
    fprintf(f, "};\n\n");
}

bool bc_generate_source(BCContext context, FILE *f) {
    bc_generate_prelude(f);

    vector_foreach(BCType, aggregate_ptr, context->aggregates) {
        fprintf(f, "typedef struct %.*s %.*s;\n",
                strp((*aggregate_ptr)->name), strp((*aggregate_ptr)->name));
    }

    fprintf(f, "\n\n");

    vector_foreach(BCType, array_ptr, context->arrays) {
        BCType array = *array_ptr;
        fprintf(f, "typedef struct Array_%u {\n", array->emit_index);
        fprintf(f, "    u32 length;\n");
        fprintf(f, "    ");
        if (array->is_dynamic || !array->count) {
            bc_generate_type(array->element, f);
            fprintf(f, " *data;\n");
        } else {
            bc_generate_type(array->element, f);
            fprintf(f, " data[");
            bc_generate_value(array->count, f);
            fprintf(f, "];\n");
        }

        fprintf(f, "} Array_%u;\n\n", array->emit_index);
    }

    vector_foreach(BCType, aggregate_ptr, context->aggregates)
            bc_generate_aggregate(*aggregate_ptr, f);

    fprintf(f, "\n\n");

    vector_foreach(BCValue, string_ptr, context->strings) {
        BCValue string = *string_ptr;
        fprintf(f, "static string __string_%llu = {", string->string_index);
        u64 length = string->string.length;
        if (length > 0)
            length--;

        fprintf(f, ".length = %llu, .data = (u8*)\"", length);

        for (u32 i = 0; i < string->string.length; i++) {
            u8 character = string->string.data[i];
            if (character < 32 || character > 126) {
                fprintf(f, "\\x%02X", character);
            } else {
                fprintf(f, "%c", character);
            }
        }

        fprintf(f, "\"};\n");
    }

    fprintf(f, "\n\n");

    vector_foreach(BCFunction, function_ptr, context->functions) {
        bc_generate_function_type(*function_ptr, f);
        fprintf(f, ";\n");
    }

    if (context->global_size > 0)
        fprintf(f, "\nstatic char GLOBAL_VARIABLES[%d];\n\n", context->global_size);

    vector_foreach(BCFunction, function_ptr, context->functions) {
        if (!(*function_ptr)->is_extern)
            bc_generate_function(*function_ptr, f);
    }

    fprintf(f, "int main(int argc, char **argv) {\n"
               "    return __atcc_start((i32)argc, (u8**)argv);\n"
               "}\n");

    return true;
}