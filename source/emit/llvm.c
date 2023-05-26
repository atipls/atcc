#include "bytecode.h"

#ifdef USE_LLVM

#include "ati/basic.h"
#include "ati/utils.h"
#include "ati/table.h"

#include <assert.h>
#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>

typedef struct {
    BCContext bc;
    LLVMContextRef llvm;
    LLVMModuleRef module;
    LLVMValueRef function;
    LLVMBuilderRef builder;

    LLVMBasicBlockRef break_target;
    LLVMBasicBlockRef continue_target;

    LLVMValueRef globals;

    LLVMAttributeRef signext;
    LLVMAttributeRef zeroext;

    PointerTable types;
} LLVMContext;

static LLVMContext *bc_context_make(BCContext bc) {
    LLVMContext *context = make(LLVMContext);
    context->bc = bc;
    context->llvm = LLVMContextCreate();
    context->module = LLVMModuleCreateWithNameInContext("main", context->llvm);
    context->function = null;
    context->builder = LLVMCreateBuilderInContext(context->llvm);

    context->break_target = null;
    context->continue_target = null;

    context->signext = LLVMCreateEnumAttribute(context->llvm, LLVMGetEnumAttributeKindForName("signext", 7), 0);
    context->zeroext = LLVMCreateEnumAttribute(context->llvm, LLVMGetEnumAttributeKindForName("zeroext", 7), 0);

    pointer_table_create(&context->types);

    return context;
}

static LLVMTypeRef bc_convert_type_base(LLVMContext *context, BCType type) {
    if (type == bc_type_void) return LLVMVoidTypeInContext(context->llvm);
    if (type == bc_type_i8) return LLVMInt8TypeInContext(context->llvm);
    if (type == bc_type_u8) return LLVMInt8TypeInContext(context->llvm);
    if (type == bc_type_i16) return LLVMInt16TypeInContext(context->llvm);
    if (type == bc_type_u16) return LLVMInt16TypeInContext(context->llvm);
    if (type == bc_type_i32) return LLVMInt32TypeInContext(context->llvm);
    if (type == bc_type_u32) return LLVMInt32TypeInContext(context->llvm);
    if (type == bc_type_i64) return LLVMInt64TypeInContext(context->llvm);
    if (type == bc_type_u64) return LLVMInt64TypeInContext(context->llvm);
    if (type == bc_type_f32) return LLVMFloatTypeInContext(context->llvm);
    if (type == bc_type_f64) return LLVMDoubleTypeInContext(context->llvm);

    return LLVMVoidTypeInContext(context->llvm);
}

static LLVMTypeRef bc_convert_type(LLVMContext *context, BCType type);

static LLVMTypeRef bc_convert_array(LLVMContext *context, BCType type) {
    LLVMTypeRef maybe_array_type = pointer_table_get(&context->types, type);
    if (maybe_array_type) return maybe_array_type;

    LLVMTypeRef array_type = LLVMStructCreateNamed(context->llvm, "ArrayType");
    LLVMTypeRef length_type = LLVMInt32TypeInContext(context->llvm);
    LLVMTypeRef base_type = bc_convert_type(context, type->element);
    LLVMTypeRef data_type = null;

    if (type->is_dynamic || !type->count) {
        data_type = LLVMPointerType(base_type, 0);
    } else {
        data_type = LLVMArrayType(base_type, type->count->storage);
    }

    LLVMTypeRef types[] = { length_type, data_type };
    LLVMStructSetBody(array_type, types, array_length(types), 0);

    pointer_table_set(&context->types, type, array_type);

    return array_type;
}

static LLVMTypeRef bc_convert_aggregate(LLVMContext *context, BCType type) {
    LLVMTypeRef maybe_aggregate_type = pointer_table_get(&context->types, type);
    if (maybe_aggregate_type) return maybe_aggregate_type;

    cstring aggregate_name = string_to_cstring(type->name);
    LLVMTypeRef aggregate_type = LLVMGetTypeByName2(context->llvm, aggregate_name);
    if (!aggregate_type) {
        aggregate_type = LLVMStructCreateNamed(context->llvm, aggregate_name);
        LLVMTypeRef *types = make_n(LLVMTypeRef, type->num_members);
        for (u32 i = 0; i < type->num_members; i++)
            types[i] = bc_convert_type(context, type->members[i].type);
        LLVMStructSetBody(aggregate_type, types, type->num_members, 0);
    }

    pointer_table_set(&context->types, type, aggregate_type);
    return aggregate_type;
}

static LLVMTypeRef bc_convert_function(LLVMContext *context, BCType type) {
    LLVMTypeRef maybe_function_type = pointer_table_get(&context->types, type);
    if (maybe_function_type) return maybe_function_type;

    LLVMTypeRef return_type = bc_convert_type(context, type->result);
    LLVMTypeRef *param_types = make_n(LLVMTypeRef, type->num_params);
    for (u32 i = 0; i < type->num_params; i++)
        param_types[i] = bc_convert_type(context, type->params[i]);
    LLVMTypeRef function_type = LLVMFunctionType(return_type, param_types, type->num_params, type->is_variadic);

    pointer_table_set(&context->types, type, function_type);
    return function_type;
}

static LLVMTypeRef bc_convert_type(LLVMContext *context, BCType type) {
    if (!type) return LLVMVoidTypeInContext(context->llvm);

    switch (type->kind) {
        case BC_TYPE_BASE: return bc_convert_type_base(context, type);
        case BC_TYPE_POINTER: return LLVMPointerType(bc_convert_type(context, type->base), 0);
        case BC_TYPE_ARRAY: return bc_convert_array(context, type);
        case BC_TYPE_FUNCTION: return bc_convert_function(context, type);
        case BC_TYPE_AGGREGATE: return bc_convert_aggregate(context, type);
        default:
            return LLVMVoidTypeInContext(context->llvm);
    }
}

#define reg(name) code->reg##name

#define regA reg(A)
#define regB reg(B)
#define regD reg(D)

static LLVMValueRef bc_generate_value(LLVMContext *context, BCValue value) {
    switch (value->kind) {
        case BC_VALUE_CONSTANT: {
            LLVMTypeRef type = bc_convert_type(context, value->type);
            assert (value->type->kind == BC_TYPE_BASE);
            if (value->type == bc_type_i8) return LLVMConstInt(type, value->istorage, 1);
            if (value->type == bc_type_u8) return LLVMConstInt(type, value->storage, 0);
            if (value->type == bc_type_i16) return LLVMConstInt(type, value->istorage, 1);
            if (value->type == bc_type_u16) return LLVMConstInt(type, value->storage, 0);
            if (value->type == bc_type_i32) return LLVMConstInt(type, value->istorage, 1);
            if (value->type == bc_type_u32) return LLVMConstInt(type, value->storage, 0);
            if (value->type == bc_type_i64) return LLVMConstInt(type, value->istorage, 1);
            if (value->type == bc_type_u64) return LLVMConstInt(type, value->storage, 0);
            if (value->type == bc_type_f32) return LLVMConstReal(type, value->floating);
            if (value->type == bc_type_f64) return LLVMConstReal(type, value->floating);

            return LLVMGetPoison(type);
        }
        case BC_VALUE_STRING: {
            cstring string_value = string_to_cstring(value->string);

            LLVMValueRef string = LLVMBuildGlobalStringPtr(context->builder, string_value, "string");
            LLVMValueRef string_length = LLVMConstInt(LLVMInt32TypeInContext(context->llvm), value->string.length - 1, 0);

            LLVMValueRef aggregate = LLVMGetUndef(bc_convert_type(context, value->type->base));

            aggregate = LLVMBuildInsertValue(context->builder, aggregate, string_length, 0, "");
            aggregate = LLVMBuildInsertValue(context->builder, aggregate, string, 1, "");

            LLVMValueRef global = LLVMAddGlobal(context->module, LLVMTypeOf(aggregate), "");
            LLVMSetInitializer(global, aggregate);

            return global;
        }
        case BC_VALUE_PARAMETER: return LLVMGetParam(context->function, value->storage);
        case BC_VALUE_TEMPORARY:
        case BC_VALUE_LOCAL: return value->backend_data;
        case BC_VALUE_PHI: return value->phi_result->backend_data;
        case BC_VALUE_GLOBAL: {
            LLVMValueRef global_index = LLVMConstInt(LLVMInt32TypeInContext(context->llvm), value->storage, 0);
            LLVMValueRef global_offset = LLVMBuildGEP2(context->builder, LLVMInt8TypeInContext(context->llvm),
                                                       context->globals, &global_index, 1, "");
            return LLVMBuildBitCast(context->builder, global_offset, bc_convert_type(context, value->type), "");
        }
        case BC_VALUE_FUNCTION: {
            BCFunction function = (BCFunction) value->storage;
            return function->backend_data;
        }
    }
}

static LLVMValueRef bc_generate_load(LLVMContext *context, BCCode code) {
    LLVMValueRef pointer = bc_generate_value(context, regA);
    LLVMTypeRef type = bc_convert_type(context, regD->type);

    return regD->backend_data = LLVMBuildLoad2(context->builder, type, pointer, "v");
}

static LLVMValueRef bc_generate_store(LLVMContext *context, BCCode code) {
    LLVMValueRef value = bc_generate_value(context, regA);
    LLVMValueRef pointer = bc_generate_value(context, regD);

    return LLVMBuildStore(context->builder, value, pointer);
}

static LLVMValueRef bc_generate_get_index(LLVMContext *context, BCCode code) {
    LLVMValueRef array = bc_generate_value(context, regA);
    LLVMValueRef index = bc_generate_value(context, regB);

    LLVMTypeRef type = bc_convert_type(context, regA->type->base);
    return regD->backend_data = LLVMBuildGEP2(context->builder, type, array, &index, 1, "get_index");
}

static LLVMValueRef bc_generate_get_field(LLVMContext *context, BCCode code) {
    // TODO: Support unions.
    LLVMValueRef aggregate = bc_generate_value(context, regA);
    LLVMTypeRef type = bc_convert_type(context, regA->type->base);

    return regD->backend_data = LLVMBuildStructGEP2(context->builder, type, aggregate, regB->storage, "v");
}

static LLVMValueRef bc_generate_add(LLVMContext *context, BCCode code) {
    // TODO: Look into NSW/NUW additions?
    LLVMValueRef lhs = bc_generate_value(context, regA);
    LLVMValueRef rhs = bc_generate_value(context, regB);
    LLVMValueRef result = regA->type->is_floating ? LLVMBuildFAdd(context->builder, lhs, rhs, "v")
                                                  : LLVMBuildAdd(context->builder, lhs, rhs, "v");

    return regD->backend_data = result;
}

static LLVMValueRef bc_generate_sub(LLVMContext *context, BCCode code) {
    // TODO: Look into NSW/NUW subtractions?
    if (!regB) {
        LLVMValueRef lhs = bc_generate_value(context, regA);
        LLVMValueRef result = regA->type->is_floating ? LLVMBuildFNeg(context->builder, lhs, "v")
                                                      : LLVMBuildNeg(context->builder, lhs, "v");

        return regD->backend_data = result;
    }

    LLVMValueRef lhs = bc_generate_value(context, regA);
    LLVMValueRef rhs = bc_generate_value(context, regB);

    LLVMValueRef result = regA->type->is_floating ? LLVMBuildFSub(context->builder, lhs, rhs, "v")
                                                  : LLVMBuildSub(context->builder, lhs, rhs, "v");

    return regD->backend_data = result;
}

static LLVMValueRef bc_generate_mul(LLVMContext *context, BCCode code) {
    LLVMValueRef lhs = bc_generate_value(context, regA);
    LLVMValueRef rhs = bc_generate_value(context, regB);
    LLVMValueRef result = regA->type->is_floating ? LLVMBuildFMul(context->builder, lhs, rhs, "v")
                                                  : LLVMBuildMul(context->builder, lhs, rhs, "v");

    return regD->backend_data = result;
}

static LLVMValueRef bc_generate_div(LLVMContext *context, BCCode code) {
    LLVMValueRef lhs = bc_generate_value(context, regA);
    LLVMValueRef rhs = bc_generate_value(context, regB);
    LLVMValueRef result = regA->type->is_floating ? LLVMBuildFDiv(context->builder, lhs, rhs, "v")
                                                  : regA->type->is_signed
                                                    ? LLVMBuildSDiv(context->builder, lhs, rhs, "v")
                                                    : LLVMBuildUDiv(context->builder, lhs, rhs, "v");
    return regD->backend_data = result;
}

static LLVMValueRef bc_generate_mod(LLVMContext *context, BCCode code) {
    LLVMValueRef lhs = bc_generate_value(context, regA);
    LLVMValueRef rhs = bc_generate_value(context, regB);
    LLVMValueRef result = regA->type->is_floating ? LLVMBuildFRem(context->builder, lhs, rhs, "v")
                                                  : regA->type->is_signed
                                                    ? LLVMBuildSRem(context->builder, lhs, rhs, "v")
                                                    : LLVMBuildURem(context->builder, lhs, rhs, "v");
    return regD->backend_data = result;
}

static LLVMValueRef bc_generate_neg(LLVMContext *context, BCCode code) {
    LLVMValueRef value = bc_generate_value(context, regA);
    LLVMValueRef result = LLVMBuildNot(context->builder, value, "v");
    return regD->backend_data = result;
}

static LLVMValueRef bc_generate_not(LLVMContext *context, BCCode code) {
    LLVMValueRef value = bc_generate_value(context, regA);
    LLVMValueRef result = LLVMBuildICmp(context->builder, LLVMIntEQ, value, LLVMConstNull(LLVMTypeOf(value)), "v");

    // Upcast to the correct type.
    LLVMTypeRef type = bc_convert_type(context, regA->type);
    result = LLVMBuildZExt(context->builder, result, type, "v");

    return regD->backend_data = result;
}

static LLVMValueRef bc_generate_and(LLVMContext *context, BCCode code) {
    LLVMValueRef lhs = bc_generate_value(context, regA);
    LLVMValueRef rhs = bc_generate_value(context, regB);
    LLVMValueRef result = LLVMBuildAnd(context->builder, lhs, rhs, "v");
    return regD->backend_data = result;
}

static LLVMValueRef bc_generate_or(LLVMContext *context, BCCode code) {
    LLVMValueRef lhs = bc_generate_value(context, regA);
    LLVMValueRef rhs = bc_generate_value(context, regB);
    LLVMValueRef result = LLVMBuildOr(context->builder, lhs, rhs, "v");
    return regD->backend_data = result;
}

static LLVMValueRef bc_generate_xor(LLVMContext *context, BCCode code) {
    LLVMValueRef lhs = bc_generate_value(context, regA);
    LLVMValueRef rhs = bc_generate_value(context, regB);
    LLVMValueRef result = LLVMBuildXor(context->builder, lhs, rhs, "v");
    return regD->backend_data = result;
}

static LLVMValueRef bc_generate_shl(LLVMContext *context, BCCode code) {
    LLVMValueRef lhs = bc_generate_value(context, regA);
    LLVMValueRef rhs = bc_generate_value(context, regB);
    LLVMValueRef result = LLVMBuildShl(context->builder, lhs, rhs, "v");
    return regD->backend_data = result;
}

static LLVMValueRef bc_generate_shr(LLVMContext *context, BCCode code) {
    LLVMValueRef lhs = bc_generate_value(context, regA);
    LLVMValueRef rhs = bc_generate_value(context, regB);
    LLVMValueRef result = regA->type->is_signed ? LLVMBuildAShr(context->builder, lhs, rhs, "v")
                                                : LLVMBuildLShr(context->builder, lhs, rhs, "v");
    return regD->backend_data = result;
}

static LLVMValueRef bc_generate_comparison(LLVMContext *context, BCCode code, int op_signed, int op_unsigned, int op_float) {
    LLVMValueRef lhs = bc_generate_value(context, regA);
    LLVMValueRef rhs = bc_generate_value(context, regB); // regB ? bc_generate_value(context, regB) : LLVMConstNull(bc_convert_type(context, regA->type));

    LLVMValueRef result = regA->type->is_floating ? LLVMBuildFCmp(context->builder, op_float, lhs, rhs, "v")
                                                  : regA->type->is_signed
                                                    ? LLVMBuildICmp(context->builder, op_signed, lhs, rhs, "v")
                                                    : LLVMBuildICmp(context->builder, op_unsigned, lhs, rhs, "v");

    // Upcast to i32 from i1
    result = LLVMBuildZExt(context->builder, result, LLVMInt32TypeInContext(context->llvm), "v");

    if (regA->type->is_floating)
        result = LLVMBuildSIToFP(context->builder, result, LLVMFloatTypeInContext(context->llvm), "v");

    return regD->backend_data = result;
}

static LLVMValueRef bc_generate_jump(LLVMContext *context, BCCode code) {
    LLVMBasicBlockRef target = code->bbT->backend_data;

    return LLVMBuildBr(context->builder, target);
}

static LLVMValueRef bc_generate_jump_if(LLVMContext *context, BCCode code) {
    LLVMValueRef condition = bc_generate_value(context, code->regC);
    LLVMBasicBlockRef block_then = code->bbT->backend_data;
    LLVMBasicBlockRef block_else = code->bbF->backend_data;

    // TODO: This is a hack to get around LLVM's requirement that the condition be an i1
    condition = LLVMBuildICmp(context->builder, LLVMIntNE, condition, LLVMConstNull(LLVMTypeOf(condition)), "v");

    return LLVMBuildCondBr(context->builder, condition, block_then, block_else);
}

static LLVMValueRef bc_generate_phi(LLVMContext *context, BCCode code) {
    LLVMTypeRef type = bc_convert_type(context, code->phi_value->type);
    LLVMValueRef result = LLVMBuildPhi(context->builder, type, "v");

    LLVMValueRef *values = make_n(LLVMValueRef, code->phi_value->num_phi);
    LLVMBasicBlockRef *blocks = make_n(LLVMBasicBlockRef, code->phi_value->num_phi);

    for (u32 i = 0; i < code->phi_value->num_phi; i++) {
        values[i] = bc_generate_value(context, code->phi_value->phi_values[i]);
        blocks[i] = code->phi_value->phi_blocks[i]->backend_data;
    }

    LLVMAddIncoming(result, values, blocks, code->phi_value->num_phi);

    return code->phi_value->phi_result->backend_data = result;
}

static LLVMValueRef bc_generate_call(LLVMContext *context, BCCode code) {
    LLVMValueRef target = bc_generate_value(context, code->target);
    LLVMTypeRef type = bc_convert_type(context, code->target->type);
    if (!LLVMIsAFunction(target))
        target = LLVMBuildLoad2(context->builder, type, target, "v");

    LLVMValueRef *args = make_n(LLVMValueRef, code->num_args);
    for (u32 i = 0; i < code->num_args; i++)
        args[i] = bc_generate_value(context, code->args[i]);

    LLVMValueRef result = LLVMBuildCall2(context->builder, type, target, args, code->num_args, "");

    return code->result->backend_data = result;
}

static LLVMValueRef bc_generate_return(LLVMContext *context, BCCode code) {
    if (!regA) return LLVMBuildRetVoid(context->builder);

    LLVMValueRef value = bc_generate_value(context, regA);

    return LLVMBuildRet(context->builder, value);
}

static LLVMValueRef bc_generate_cast(LLVMContext *context, BCCode code, int opcode) {
    LLVMValueRef value = bc_generate_value(context, regA);
    LLVMTypeRef type = bc_convert_type(context, regD->type);

    LLVMValueRef result = LLVMBuildCast(context->builder, opcode, value, type, "v");

    return regD->backend_data = result;
}

static LLVMValueRef bc_generate_code(LLVMContext *context, BCCode code) {
// clang-format off
    switch (code->opcode) {
        case BC_OP_NOP: return null;
        case BC_OP_LOAD: return bc_generate_load(context, code);
        case BC_OP_STORE: return bc_generate_store(context, code);
        case BC_OP_GET_INDEX: return bc_generate_get_index(context, code);
        case BC_OP_GET_FIELD: return bc_generate_get_field(context, code);
        case BC_OP_ADD: return bc_generate_add(context, code);
        case BC_OP_SUB: return bc_generate_sub(context, code);
        case BC_OP_MUL: return bc_generate_mul(context, code);
        case BC_OP_DIV: return bc_generate_div(context, code);
        case BC_OP_MOD: return bc_generate_mod(context, code);
        case BC_OP_NEG: return bc_generate_neg(context, code);
        case BC_OP_NOT: return bc_generate_not(context, code);
        case BC_OP_AND: return bc_generate_and(context, code);
        case BC_OP_OR: return bc_generate_or(context, code);
        case BC_OP_XOR: return bc_generate_xor(context, code);
        case BC_OP_SHL: return bc_generate_shl(context, code);
        case BC_OP_SHR: return bc_generate_shr(context, code);
        case BC_OP_EQ: return bc_generate_comparison(context, code, LLVMIntEQ, LLVMIntEQ, LLVMRealOEQ);
        case BC_OP_NE: return bc_generate_comparison(context, code, LLVMIntNE, LLVMIntNE, LLVMRealONE);
        case BC_OP_LT: return bc_generate_comparison(context, code, LLVMIntSLT, LLVMIntULT, LLVMRealOLT);
        case BC_OP_GT: return bc_generate_comparison(context, code, LLVMIntSGT, LLVMIntUGT, LLVMRealOGT);
        case BC_OP_LE: return bc_generate_comparison(context, code, LLVMIntSLE, LLVMIntULE, LLVMRealOLE);
        case BC_OP_GE: return bc_generate_comparison(context, code, LLVMIntSGE, LLVMIntUGE, LLVMRealOGE);
        case BC_OP_JUMP: return bc_generate_jump(context, code);
        case BC_OP_JUMP_IF: return bc_generate_jump_if(context, code);
        case BC_OP_PHI: return bc_generate_phi(context, code);
        case BC_OP_CALL: return bc_generate_call(context, code);
        case BC_OP_RETURN: return bc_generate_return(context, code);
        case BC_OP_CAST_BITWISE: return bc_generate_cast(context, code, LLVMBitCast);
        case BC_OP_CAST_INT_TO_PTR: return bc_generate_cast(context, code, LLVMIntToPtr);
        case BC_OP_CAST_PTR_TO_INT: return bc_generate_cast(context, code, LLVMPtrToInt);
        case BC_OP_CAST_INT_TRUNC: return bc_generate_cast(context, code, LLVMTrunc);
        case BC_OP_CAST_INT_ZEXT: return bc_generate_cast(context, code, LLVMZExt);
        case BC_OP_CAST_INT_SEXT: return bc_generate_cast(context, code, LLVMSExt);
        case BC_OP_CAST_FP_EXTEND: return bc_generate_cast(context, code, LLVMFPExt);
        case BC_OP_CAST_FP_TRUNC: return bc_generate_cast(context, code, LLVMFPTrunc);
        case BC_OP_CAST_FP_TO_SINT: return bc_generate_cast(context, code, LLVMFPToSI);
        case BC_OP_CAST_FP_TO_UINT: return bc_generate_cast(context, code, LLVMFPToUI);
        case BC_OP_CAST_SINT_TO_FP: return bc_generate_cast(context, code, LLVMSIToFP);
        case BC_OP_CAST_UINT_TO_FP: return bc_generate_cast(context, code, LLVMUIToFP);
        default: assert(!"unimplemented opcode"); return null;
    }
// clang-format on
}

static void bc_generate_function_type(LLVMContext *context, BCFunction function) {
    unsigned index = LLVMLookupIntrinsicID((cstring)function->name.data, function->name.length);
    if (index != 0) {
        LLVMTypeRef *param_types = make_n(LLVMTypeRef, function->signature->num_params);
        for (u32 i = 0; i < function->signature->num_params; i++)
            param_types[i] = bc_convert_type(context, function->signature->params[i]);
        function->backend_data = LLVMGetIntrinsicDeclaration(context->module, index, param_types, function->signature->num_params);
        return;
    }

    BCType bc_return_type = function->signature->result;
    LLVMTypeRef return_type = bc_convert_type(context, bc_return_type);
    LLVMTypeRef *param_types = make_n(LLVMTypeRef, function->signature->num_params);

    for (u32 i = 0; i < function->signature->num_params; i++)
        param_types[i] = bc_convert_type(context, function->signature->params[i]);

    LLVMTypeRef function_type = LLVMFunctionType(return_type, param_types, function->signature->num_params,
                                                 function->signature->is_variadic);

    cstring function_name = string_to_cstring(function->name);
    LLVMValueRef function_value = LLVMAddFunction(context->module, function_name, function_type);

    LLVMSetLinkage(function_value, LLVMExternalLinkage);
    LLVMSetFunctionCallConv(function_value, LLVMCCallConv);

    if (bc_return_type && bc_return_type != bc_type_void && bc_return_type->size < 4) {
        LLVMAttributeRef attribute = bc_return_type->is_signed ? context->signext : context->zeroext;
        LLVMAddAttributeAtIndex(function_value, LLVMAttributeReturnIndex, attribute);
    }

    function->backend_data = function_value;
}

void bc_dump_code(BCCode code, FILE *f);

static void bc_generate_function(LLVMContext *context, BCFunction function) {
    LLVMValueRef function_value = function->backend_data;
    LLVMBasicBlockRef entry_block = LLVMAppendBasicBlockInContext(context->llvm, function_value, "entry");
    for (BCBlock block = function->first_block; block; block = block->next)
        block->backend_data = LLVMCreateBasicBlockInContext(context->llvm, "block");

    context->function = function_value;
    context->break_target = null;
    context->continue_target = null;

    LLVMPositionBuilderAtEnd(context->builder, entry_block);
    vector_foreach(BCValue, local_ptr, function->locals) {
        BCValue local = *local_ptr;
        local->backend_data = LLVMBuildAlloca(context->builder, bc_convert_type(context, local->type->base), "local");
    }

    LLVMBuildBr(context->builder, function->first_block->backend_data);

    for (BCBlock block = function->first_block; block; block = block->next) {
        LLVMAppendExistingBasicBlock(function_value, block->backend_data);
        LLVMPositionBuilderAtEnd(context->builder, block->backend_data);

        vector_foreach(BCCode, code, block->code) {
#if 0
            bc_dump_code(*code, stdout);
            printf("\n");
            fflush(stdout);
#endif
#if 0
            LLVMDumpValue(bc_generate_code(context, *code));
            printf("\n");
            fflush(stdout);
#endif
#if 1
            bc_generate_code(context, *code);
#endif
        }
    }

    if (!function->signature->result || function->signature->result == bc_type_void) {
        LLVMBuildRetVoid(context->builder);
    }

    if (LLVMVerifyFunction(function_value, LLVMPrintMessageAction)) {
        fflush(stderr);
        printf("\n\n\nRAW LLVM:\n\n\n");
        LLVMDumpValue(function_value);
        printf("\n\n\n");
        fflush(stdout);

        printf("\n\n\nBC:\n\n\n");
        bc_dump_function(function, stdout);
        printf("\n\n\n");
        fflush(stdout);
    }

    fflush(stdout);
}

bool bc_generate_llvm(BCContext bc, string file) {
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();

    LLVMContext *context = bc_context_make(bc);

    if (bc->global_size > 0) {
        context->globals = LLVMAddGlobal(context->module,
                                         LLVMArrayType(LLVMInt8TypeInContext(context->llvm), bc->global_size),
                                         "globals");
        LLVMSetLinkage(context->globals, LLVMExternalLinkage);
        LLVMSetInitializer(context->globals, LLVMConstNull(LLVMGlobalGetValueType(context->globals)));
    }

    vector_foreach(BCFunction, function_ptr, bc->functions) {
        bc_generate_function_type(context, *function_ptr);
    }

    vector_foreach(BCFunction, function_ptr, bc->functions) {
        if (!(*function_ptr)->is_extern)
            bc_generate_function(context, *function_ptr);
    }

    char *errors = null;
    LLVMPrintModuleToFile(context->module, "result.ll", &errors);


    bool intel = false;
    cstring triple = intel ? "x86_64-pc-linux-gnu" : LLVMGetDefaultTargetTriple();
    cstring cpu_name = intel ? "alderlake" : LLVMGetHostCPUName();
    cstring features = intel ? "" : LLVMGetHostCPUFeatures();

    LLVMTargetRef target;
    LLVMGetTargetFromTriple(triple, &target, &errors);
    LLVMTargetMachineRef machine = LLVMCreateTargetMachine(target, triple, cpu_name, features, LLVMCodeGenLevelAggressive, LLVMRelocStatic, LLVMCodeModelDefault);

    LLVMSetTarget(context->module, triple);
    LLVMTargetDataRef datalayout = LLVMCreateTargetDataLayout(machine);
    char *datalayout_str = LLVMCopyStringRepOfTargetData(datalayout);
    LLVMSetDataLayout(context->module, datalayout_str);
    LLVMDisposeMessage(datalayout_str);

    cstring output_file = string_to_cstring(file);
    LLVMTargetMachineEmitToFile(machine, context->module, output_file, LLVMObjectFile, &errors);

    if (errors) LLVMDisposeMessage(errors);

    return true;
}

#else

bool bc_generate_llvm(BCContext context, string file) {
    (void) context;
    (void) file;

    printf("LLVM Support not enabled.\n");
    return false;
}

#endif