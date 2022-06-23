#include "bytecode.h"

#include "atcc.h"
#include "ati/utils.h"

BCValue bc_value_make(BCFunction function, BCType type) {
    BCValue value = make(struct SBCValue);

    value->type = type;
    value->flags = BC_VALUE_IS_TEMPORARY;
    value->storage = function->last_temporary++;

    return value;
}

BCValue bc_value_make_consti(BCFunction function, BCType type, u64 value) {
    BCValue constant = bc_value_make(function, type);
    assert(type != bc_type_void);
    assert(!type->is_floating);

    constant->flags = BC_VALUE_IS_CONSTANT;
    constant->storage = value;
    return constant;
}

BCValue bc_value_make_constf(BCFunction function, BCType type, f64 value) {
    BCValue constant = bc_value_make(function, type);
    assert(type != bc_type_void);
    assert(type->is_floating);

    constant->flags = BC_VALUE_IS_CONSTANT;
    constant->floating = value;
    return constant;
}

BCValue bc_value_get_parameter(BCFunction function, u32 index) {
    if (!function->params) {
        function->params = make_n(struct SBCValue, function->signature->num_params);
        for (u64 i = 0; i < function->signature->num_params; i++) {
            BCValue param = &function->params[i];

            param->flags |= BC_VALUE_IS_PARAMETER;
            param->type = function->signature->params[i];
            param->storage = i;
        }
    }

    return &function->params[index];
}


#define BASE_TYPE(_size, _is_signed, ...) \
    &(struct SBCType) { .kind = BC_TYPE_BASE, .size = _size, .alignment = _size, .is_signed = _is_signed, __VA_ARGS__ }

BCType bc_type_void = BASE_TYPE(0, false);
BCType bc_type_i8 = BASE_TYPE(1, true), bc_type_u8 = BASE_TYPE(1, false);
BCType bc_type_i16 = BASE_TYPE(2, true), bc_type_u16 = BASE_TYPE(2, false);
BCType bc_type_i32 = BASE_TYPE(4, true), bc_type_u32 = BASE_TYPE(4, false);
BCType bc_type_i64 = BASE_TYPE(8, true), bc_type_u64 = BASE_TYPE(8, false);
BCType bc_type_f32 = BASE_TYPE(4, true, .is_floating = true),
       bc_type_f64 = BASE_TYPE(8, true, .is_floating = true);

#undef BASE_TYPE

BCType bc_type_pointer(BCType type) {
    BCType pointer = make(struct SBCType);
    pointer->kind = BC_TYPE_POINTER;
    pointer->size = pointer->alignment = POINTER_SIZE;
    pointer->subtype = type;
    return pointer;
}

BCType bc_type_array(BCType type, BCValue size) {
    BCType array = make(struct SBCType);
    array->kind = BC_TYPE_ARRAY;
    array->size = array->alignment = POINTER_SIZE; // TODO: This should be a fat pointer, ptr+size
    array->subtype = type;
    array->count = size;
    return array;
}

BCType bc_type_function(BCType result, BCType *params, u32 num_params) {
    BCType function = make(struct SBCType);
    function->kind = BC_TYPE_FUNCTION;
    function->size = function->alignment = POINTER_SIZE;
    function->subtype = result;
    function->params = params;
    function->num_params = num_params;
    return function;
}

bool bc_type_is_integer(BCType type) {
    return type == bc_type_i8 || type == bc_type_i16 || type == bc_type_i32 || type == bc_type_i64 ||
           type == bc_type_u8 || type == bc_type_u16 || type == bc_type_u32 || type == bc_type_u64;
}

BCContext bc_context_initialize() {
    BCContext context = make(struct SBCContext);
    // lol
    return context;
}

BCFunction bc_function_create(BCContext context, BCType signature, string name) {
    BCFunction function = make(struct SBCFunction);

    function->signature = signature;
    function->name = name;

    BCBlock initial_block = bc_block_make(function);
    function->first_block = function->last_block = function->current_block = initial_block;

    vector_push(context->functions, function);

    return function;
}

BCBlock bc_function_set_block(BCFunction function, BCBlock block) {
    BCBlock last_block = function->current_block;
    function->current_block = block;
    return last_block;
}

BCBlock bc_function_get_block(BCFunction function) {
    return function->current_block;
}

BCValue bc_function_define(BCFunction function, BCType type) {
    BCValue value = make(struct SBCValue);
    value->flags = BC_VALUE_IS_ON_STACK;
    value->type = bc_type_pointer(type);
    value->storage = function->stack_size;
    function->stack_size += type->size;
    return value;
}

BCBlock bc_block_make(BCFunction function) {
    BCBlock block = make(struct SBCBlock);
    block->serial = function->last_block_serial++;

    if (function->last_block) {
        function->last_block->next = block;
        block->prev = function->last_block;
        function->last_block = block;
    }

    return block;
}

bool bc_block_is_terminated(BCBlock block) {
    if (!vector_len(block->code)) return false;
    BCCode last_insn = vector_last(block->code);
    return last_insn->opcode == BC_OP_RETURN ||
           last_insn->opcode == BC_OP_JUMP ||
           last_insn->opcode == BC_OP_JUMP_IF;
}

BCCode bc_insn_make(BCBlock block) {
    BCCode code = make(struct SBCCode);

    code->block = block;

    vector_push(block->code, code);
    return code;
}

static BCBlock bc_insn_current_block(BCFunction function) {
    if (!function->current_block)
        return bc_function_get_block(function);
    return function->current_block;
}

static BCCode bc_insn_of(BCFunction function) {
    BCBlock block = bc_insn_current_block(function);
    return bc_insn_make(block);
}

#define BC_TODO()                \
    assert(!"UNIMPLEMENTED BC"); \
    return null;

BCValue bc_insn_nop(BCFunction function) {
    BCCode insn = bc_insn_of(function);
    insn->opcode = BC_OP_NOP;
    return null;
}

BCValue bc_insn_load(BCFunction function, BCValue source) {
    if (source->flags & BC_VALUE_IS_CONSTANT)
        return source;
    assert(source->type->kind == BC_TYPE_POINTER);

    BCCode insn = bc_insn_of(function);
    insn->opcode = BC_OP_LOAD;
    insn->regA = source;
    insn->regD = bc_value_make(function, source->type->subtype);

    return insn->regD;
}

BCValue bc_insn_load_address(BCFunction function, BCValue source) { BC_TODO(); }

BCValue bc_insn_store(BCFunction function, BCValue dest, BCValue source) {
    BCCode insn = bc_insn_of(function);
    insn->opcode = BC_OP_STORE;
    insn->regA = source;
    insn->regD = dest;

    return insn->regD;
}

BCCode bc_insn_jump(BCFunction function, BCBlock block) {
    // TODO: Terminate the current block and start a new one.
    BCCode insn = bc_insn_of(function);
    insn->opcode = BC_OP_JUMP;
    insn->bbT = block;

    return insn;
}

BCCode bc_insn_jump_if(BCFunction function, BCValue cond, BCBlock block_true, BCBlock block_false) {
    // TODO: Terminate the current block and start a new one.
    BCCode insn = bc_insn_of(function);
    insn->opcode = BC_OP_JUMP_IF;
    insn->regC = cond;
    insn->bbT = block_true;
    insn->bbF = block_false;

    return insn;
}

static BCValue bc_insn_arith(BCFunction function, BCOpcode opcode, BCValue arg1, BCValue arg2) {
    // TODO: Check if the value is constant, return a constant.
    // TODO: Better type equality check.
    // assert(arg1->type == arg2->type);

    BCCode insn = bc_insn_of(function);
    insn->opcode = opcode;
    insn->regA = arg1;
    insn->regB = arg2;
    insn->regD = bc_value_make(function, arg1->type);

    return insn->regD;
}

BCValue bc_insn_add(BCFunction function, BCValue arg1, BCValue arg2) { return bc_insn_arith(function, BC_OP_ADD, arg1, arg2); }
BCValue bc_insn_sub(BCFunction function, BCValue arg1, BCValue arg2) { return bc_insn_arith(function, BC_OP_SUB, arg1, arg2); }
BCValue bc_insn_mul(BCFunction function, BCValue arg1, BCValue arg2) { return bc_insn_arith(function, BC_OP_MUL, arg1, arg2); }
BCValue bc_insn_div(BCFunction function, BCValue arg1, BCValue arg2) { return bc_insn_arith(function, BC_OP_DIV, arg1, arg2); }
BCValue bc_insn_mod(BCFunction function, BCValue arg1, BCValue arg2) { return bc_insn_arith(function, BC_OP_MOD, arg1, arg2); }
BCValue bc_insn_neg(BCFunction function, BCValue arg1, BCValue arg2) { return bc_insn_arith(function, BC_OP_NEG, arg1, arg2); }
BCValue bc_insn_not(BCFunction function, BCValue arg1, BCValue arg2) { return bc_insn_arith(function, BC_OP_NOT, arg1, arg2); }
BCValue bc_insn_and(BCFunction function, BCValue arg1, BCValue arg2) { return bc_insn_arith(function, BC_OP_AND, arg1, arg2); }
BCValue bc_insn_or(BCFunction function, BCValue arg1, BCValue arg2) { return bc_insn_arith(function, BC_OP_OR, arg1, arg2); }
BCValue bc_insn_xor(BCFunction function, BCValue arg1, BCValue arg2) { return bc_insn_arith(function, BC_OP_XOR, arg1, arg2); }
BCValue bc_insn_shl(BCFunction function, BCValue arg1, BCValue arg2) { return bc_insn_arith(function, BC_OP_SHL, arg1, arg2); }
BCValue bc_insn_shr(BCFunction function, BCValue arg1, BCValue arg2) { return bc_insn_arith(function, BC_OP_SHR, arg1, arg2); }
BCValue bc_insn_eq(BCFunction function, BCValue arg1, BCValue arg2) { return bc_insn_arith(function, BC_OP_EQ, arg1, arg2); }
BCValue bc_insn_ne(BCFunction function, BCValue arg1, BCValue arg2) { return bc_insn_arith(function, BC_OP_NE, arg1, arg2); }
BCValue bc_insn_lt(BCFunction function, BCValue arg1, BCValue arg2) { return bc_insn_arith(function, BC_OP_LT, arg1, arg2); }
BCValue bc_insn_gt(BCFunction function, BCValue arg1, BCValue arg2) { return bc_insn_arith(function, BC_OP_GT, arg1, arg2); }
BCValue bc_insn_le(BCFunction function, BCValue arg1, BCValue arg2) { return bc_insn_arith(function, BC_OP_LE, arg1, arg2); }
BCValue bc_insn_ge(BCFunction function, BCValue arg1, BCValue arg2) { return bc_insn_arith(function, BC_OP_GE, arg1, arg2); }

BCValue bc_insn_call(BCFunction function, BCValue target, BCValue *args, u32 num_args) {
    BCCode insn = bc_insn_of(function);
    insn->opcode = BC_OP_CALL;

    insn->target = target;
    insn->result = bc_value_make(function, target->type->subtype);
    insn->args = args;
    insn->num_args = num_args;

    return insn->result;
}

BCValue bc_insn_return(BCFunction function, BCValue value) {
    // TODO: Terminate the current block and start a new one.
    // TODO: Check if the return type is null, or if it's of a void type.

    BCCode insn = bc_insn_of(function);
    insn->opcode = BC_OP_RETURN;
    insn->regA = value;

    return null;
}

BCValue bc_insn_cast(BCFunction function, BCOpcode opcode, BCValue source, BCType target) {
    BCCode insn = bc_insn_of(function);
    insn->opcode = opcode;
    insn->regA = source;
    insn->regD = bc_value_make(function, target);

    return insn->regD;
}