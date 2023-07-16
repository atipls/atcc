#include "bytecode.h"
#include <string.h>

#include "atcc.h"
#include "ati/utils.h"

#ifdef _WIN32
#include <stdlib.h>
#define bswap_16(x) _byteswap_ushort(x)
#define bswap_32(x) _byteswap_ulong(x)
#define bswap_64(x) _byteswap_uint64(x)
#else
#define bswap_16(x) __builtin_bswap16(x)
#define bswap_32(x) __builtin_bswap32(x)
#define bswap_64(x) __builtin_bswap64(x)
#endif

BCValue bc_value_make(BCFunction function, BCType type) {
    BCValue value = make(struct SBCValue);

    value->type = type;
    value->kind = BC_VALUE_TEMPORARY;
    value->storage = function->last_temporary++;

    return value;
}

BCValue bc_value_make_consti(BCType type, u64 value) {
    BCValue constant = make(struct SBCValue);

    assert(type != bc_type_void);
    assert(!type->is_floating);

    constant->type = type;
    constant->kind = BC_VALUE_CONSTANT;
    constant->storage = value;
    return constant;
}

BCValue bc_value_make_constf(BCType type, f64 value) {
    BCValue constant = make(struct SBCValue);

    assert(type != bc_type_void);
    assert(type->is_floating);

    constant->type = type;
    constant->kind = BC_VALUE_CONSTANT;
    constant->floating = value;
    return constant;
}

BCValue bc_value_make_string(BCContext context, BCType type, string string) {
    BCValue constant = make(struct SBCValue);

    constant->type = bc_type_pointer(type);
    constant->kind = BC_VALUE_STRING;
    constant->string = string;
    constant->string_index = vector_length(context->strings);

    vector_push(context->strings, constant);

    return constant;
}

BCValue bc_value_make_phi(BCFunction function, BCType type) {
    BCValue phi = make(struct SBCValue);

    phi->type = type;
    phi->kind = BC_VALUE_PHI;

    phi->phi_result = bc_value_make(function, type);
    phi->phi_values = vector_create(BCValue);
    phi->phi_blocks = vector_create(BCBlock);
    phi->num_incoming_phi_values = 0;

    return phi;
}

BCValue bc_value_get_parameter(BCFunction function, u32 index) {
    if (!function->params) {
        function->params = make_n(struct SBCValue, function->signature->num_params);
        for (u64 i = 0; i < function->signature->num_params; i++) {
            BCValue param = &function->params[i];

            param->kind = BC_VALUE_PARAMETER;
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
    pointer->base = type;
    return pointer;
}

BCType bc_type_array(BCContext context, BCType type, BCValue size, bool is_dynamic) {
    BCType array = make(struct SBCType);
    array->kind = BC_TYPE_ARRAY;

    if (!is_dynamic && size && size->kind == BC_VALUE_CONSTANT)
        array->size = array->alignment = type->size * (u32) size->storage + POINTER_SIZE; // TODO: Better alignment.

    if (is_dynamic || !size)
        array->size = array->alignment = POINTER_SIZE * 2; // TODO: Better alignment.

    array->element = type;
    array->count = size;
    array->is_dynamic = is_dynamic;
    array->emit_index = vector_length(context->arrays);

    vector_push(context->arrays, array);
    return array;
}

BCType bc_type_function(BCType result, BCType *params, u32 num_params) {
    BCType function = make(struct SBCType);
    function->kind = BC_TYPE_FUNCTION;
    function->size = function->alignment = POINTER_SIZE;
    function->result = result;
    function->params = params;
    function->num_params = num_params;
    return function;
}

BCType bc_type_aggregate(BCContext context, string name) {
    BCType aggregate = make(struct SBCType);
    aggregate->kind = BC_TYPE_AGGREGATE;
    aggregate->name = name;

    vector_push(context->aggregates, aggregate);
    return aggregate;
}

void bc_type_aggregate_set_body(BCType aggregate, BCAggregate *members, u32 num_members) {
    aggregate->members = members;
    aggregate->num_members = num_members;

    // TODO: Better alignment/union calculation.
    u32 total_size = 0;
    for (u32 i = 0; i < num_members; i++) {
        BCAggregate *member = &members[i];
        total_size += member->type->size;
    }

    aggregate->size = aggregate->alignment = total_size;
}

bool bc_type_is_integer(BCType type) {
    return type == bc_type_i8 || type == bc_type_i16 || type == bc_type_i32 || type == bc_type_i64 ||
           type == bc_type_u8 || type == bc_type_u16 || type == bc_type_u32 || type == bc_type_u64;
}

BCContext bc_context_initialize() {
    BCContext context = make(struct SBCContext);

    context->arrays = vector_create(BCType);
    context->aggregates = vector_create(BCType);
    context->strings = vector_create(BCValue);

    context->functions = vector_create(BCFunction);

    return context;
}

BCFunction bc_function_create(BCContext context, BCType signature, string name) {
    BCFunction function = make(struct SBCFunction);

    function->signature = signature;
    function->name = name;

    BCBlock initial_block = bc_block_make(function);
    function->first_block = function->last_block = function->current_block = initial_block;

    function->locals = vector_create(BCValue);

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
    value->kind = BC_VALUE_LOCAL;
    value->type = bc_type_pointer(type);
    value->storage = function->stack_size;
    function->stack_size += type->size;
    vector_push(function->locals, value);
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

    block->code = vector_create(BCCode);

    return block;
}

bool bc_block_is_terminated(BCBlock block) {
    if (!vector_length(block->code)) return false;
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
    if (source->kind == BC_VALUE_CONSTANT)
        return source;
    assert(source->type->kind == BC_TYPE_POINTER);

    BCCode insn = bc_insn_of(function);
    insn->opcode = BC_OP_LOAD;
    insn->regA = source;
    insn->regD = bc_value_make(function, source->type->base);

    return insn->regD;
}

BCValue bc_insn_store(BCFunction function, BCValue dest, BCValue source) {
    BCCode insn = bc_insn_of(function);
    insn->opcode = BC_OP_STORE;
    insn->regA = source;
    insn->regD = dest;

    return insn->regD;
}

BCValue bc_insn_get_field(BCFunction function, BCValue source, BCType type, u64 field) {
    BCCode insn = bc_insn_of(function);

    assert(source->type->kind == BC_TYPE_POINTER);

    insn->opcode = BC_OP_GET_FIELD;
    insn->regA = source;
    insn->regB = bc_value_make_consti(bc_type_u64, field);
    insn->regD = bc_value_make(function, bc_type_pointer(type));

    return insn->regD;
}

BCValue bc_insn_get_index(BCFunction function, BCValue source, BCType type, BCValue index) {
    BCCode insn = bc_insn_of(function);

    insn->opcode = BC_OP_GET_INDEX;
    insn->regA = source;
    insn->regB = index;
    insn->regD = bc_value_make(function, bc_type_pointer(type));

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

BCValue bc_insn_phi(BCFunction function, BCType type) {
    BCCode insn = bc_insn_of(function);

    insn->opcode = BC_OP_PHI;
    insn->phi_value = bc_value_make_phi(function, type);

    return insn->phi_value;
}

void bc_insn_phi_add_incoming(BCValue phi, BCValue *values, BCBlock *blocks, u32 num_incoming) {
    assert(phi->kind == BC_VALUE_PHI);
    assert(num_incoming > 0);

    for (u32 i = 0; i < num_incoming; i++) {
        vector_push(phi->phi_values, values[i]);
        vector_push(phi->phi_blocks, blocks[i]);

        BCCode last_insn = vector_last(blocks[i]->code);
        last_insn->regC = values[i];
    }

    phi->num_incoming_phi_values += num_incoming;
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
    insn->result = bc_value_make(function, target->type->result);
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

BCBuffer *bc_buffer_create(u64 initial_capacity) {
    BCBuffer *buffer = make(BCBuffer);

    buffer->data = malloc(initial_capacity);
    buffer->size = 0;
    buffer->capacity = initial_capacity;

    return buffer;
}

void bc_buffer_destroy(BCBuffer *buffer) {
    free(buffer->data);
    free(buffer);
}

// We should grow less linearly, but should be fine for now.
static void bc_buffer_ensure(BCBuffer *buffer, u64 size) {
    if (buffer->size + size > buffer->capacity) {
        buffer->capacity *= 2;
        buffer->data = realloc(buffer->data, buffer->capacity);
    }
}


u64 bc_emit_u8(BCBuffer *buffer, u8 value) {
    bc_buffer_ensure(buffer, 1);

    buffer->data[buffer->size++] = value;

    return buffer->size - 1;
}

u64 bc_emit_u16(BCBuffer *buffer, u16 value) {
    bc_buffer_ensure(buffer, 2);

    value = bswap_16(value);

    buffer->data[buffer->size++] = (value >> 8) & 0xFF;
    buffer->data[buffer->size++] = value & 0xFF;

    return buffer->size - 2;
}

u64 bc_emit_u32(BCBuffer *buffer, u32 value) {
    bc_buffer_ensure(buffer, 4);

    value = bswap_32(value);

    buffer->data[buffer->size++] = (value >> 24) & 0xFF;
    buffer->data[buffer->size++] = (value >> 16) & 0xFF;
    buffer->data[buffer->size++] = (value >> 8) & 0xFF;
    buffer->data[buffer->size++] = value & 0xFF;

    return buffer->size - 4;
}

u64 bc_emit_u64(BCBuffer *buffer, u64 value) {
    bc_buffer_ensure(buffer, 8);

    value = bswap_64(value);

    buffer->data[buffer->size++] = (value >> 56) & 0xFF;
    buffer->data[buffer->size++] = (value >> 48) & 0xFF;
    buffer->data[buffer->size++] = (value >> 40) & 0xFF;
    buffer->data[buffer->size++] = (value >> 32) & 0xFF;
    buffer->data[buffer->size++] = (value >> 24) & 0xFF;
    buffer->data[buffer->size++] = (value >> 16) & 0xFF;
    buffer->data[buffer->size++] = (value >> 8) & 0xFF;
    buffer->data[buffer->size++] = value & 0xFF;

    return buffer->size - 8;
}

u64 bc_emit_f32(BCBuffer *buffer, f32 value) {
    return bc_emit_u32(buffer, *(u32 *) &value);
}

u64 bc_emit_f64(BCBuffer *buffer, f64 value) {
    return bc_emit_u64(buffer, *(u64 *) &value);
}

u64 bc_emit_data(BCBuffer *buffer, u8 *data, u64 size) {
    bc_buffer_ensure(buffer, size);

    memcpy(buffer->data + buffer->size, data, size);
    buffer->size += size;

    return buffer->size - size;
}

void bc_patch_u8(BCBuffer *buffer, u64 offset, u8 value) {
    buffer->data[offset] = value;
}

void bc_patch_u16(BCBuffer *buffer, u64 offset, u16 value) {
    value = bswap_16(value);

    buffer->data[offset + 0] = (value >> 8) & 0xFF;
    buffer->data[offset + 1] = value & 0xFF;
}

void bc_patch_u32(BCBuffer *buffer, u64 offset, u32 value) {
    value = bswap_32(value);

    buffer->data[offset + 0] = (value >> 24) & 0xFF;
    buffer->data[offset + 1] = (value >> 16) & 0xFF;
    buffer->data[offset + 2] = (value >> 8) & 0xFF;
    buffer->data[offset + 3] = value & 0xFF;
}

void bc_patch_u64(BCBuffer *buffer, u64 offset, u64 value) {
    value = bswap_64(value);

    buffer->data[offset + 0] = (value >> 56) & 0xFF;
    buffer->data[offset + 1] = (value >> 48) & 0xFF;
    buffer->data[offset + 2] = (value >> 40) & 0xFF;
    buffer->data[offset + 3] = (value >> 32) & 0xFF;
    buffer->data[offset + 4] = (value >> 24) & 0xFF;
    buffer->data[offset + 5] = (value >> 16) & 0xFF;
    buffer->data[offset + 6] = (value >> 8) & 0xFF;
    buffer->data[offset + 7] = value & 0xFF;
}

void bc_patch_f32(BCBuffer *buffer, u64 offset, f32 value) {
    bc_patch_u32(buffer, offset, *(u32 *) &value);
}

void bc_patch_f64(BCBuffer *buffer, u64 offset, f64 value) {
    bc_patch_u64(buffer, offset, *(u64 *) &value);
}

