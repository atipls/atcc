#include "bytecode.h"
#include "atcc.h"

BCValue bc_value_make(BCFunction function, BCType type) {
}

BCValue bc_value_make_consti(BCFunction function, BCType type, u64 value) {
}

BCValue bc_value_make_constf(BCFunction function, BCType type, f64 value) {
}

BCValue bc_value_get_parameter(BCFunction function, u32 index) {
}


#define BASE_TYPE(_size, _is_signed, ...) \
    &(struct BCType) { .kind = BC_TYPE_BASE, .size = _size, .alignment = _size, .is_signed = _is_signed, __VA_ARGS__ }

BCType bc_type_void = BASE_TYPE(0, false);
BCType bc_type_i8 = BASE_TYPE(1, true), bc_type_u8 = BASE_TYPE(1, false);
BCType bc_type_i16 = BASE_TYPE(2, true), bc_type_u16 = BASE_TYPE(2, false);
BCType bc_type_i32 = BASE_TYPE(4, true), bc_type_u32 = BASE_TYPE(4, false);
BCType bc_type_i64 = BASE_TYPE(8, true), bc_type_u64 = BASE_TYPE(8, false);
BCType bc_type_f32 = BASE_TYPE(4, true, .is_floating = true),
       bc_type_f64 = BASE_TYPE(8, true, .is_floating = true);

#undef BASE_TYPE

BCType bc_type_pointer(BCType type) {
    BCType pointer = make(struct BCType);
    pointer->kind = BC_TYPE_POINTER;
    pointer->size = pointer->alignment = POINTER_SIZE;
    pointer->subtype = type;
    return pointer;
}

BCType bc_type_array(BCType type, BCValue size) {
    BCType array = make(struct BCType);
    array->kind = BC_TYPE_ARRAY;
    array->size = array->alignment = POINTER_SIZE;// TODO: This should be a fat pointer, ptr+size
    array->subtype = type;
    array->count = size;
    return array;
}

BCType bc_type_function(BCType result, BCType *params, u32 num_params) {
    BCType function = make(struct BCType);
    function->kind = BC_TYPE_FUNCTION;
    function->size = function->alignment = POINTER_SIZE;
    function->subtype = result;
    function->params = params;
    function->num_params = num_params;
    return function;
}

BCContext bc_context_initialize() {
    BCContext context = make(struct BCContext);
    // lol
    return context;
}

BCFunction bc_function_create(BCContext context, BCType signature, string name) {
    BCFunction function = make(struct BCFunction);

    function->signature = signature;
    function->name = name;

    return function;
}

BCBlock bc_function_set_block(BCFunction function, BCBlock block) {
}

BCBlock bc_function_get_block(BCFunction function) {
    if (!function->current_block) {
        BCBlock initial_block = bc_block_create(function);
        function->first_block = initial_block;

        function->current_block = function->first_block;
    }

    return function->current_block;
}

BCBlock bc_function_get_initializer(BCFunction function) {
}

BCBlock bc_block_create(BCFunction function) {
    BCBlock block = make(struct BCBlock);

    return block;
}

BCCode bc_insn_make(BCBlock block);

BCValue bc_insn_nop(BCFunction function);

BCValue bc_insn_load(BCFunction function, BCValue dest, BCValue src);
BCValue bc_insn_load_address(BCFunction function, BCValue dest, BCValue src);
BCValue bc_insn_store(BCFunction function, BCValue dest, BCValue src);

BCValue bc_insn_jump(BCFunction function, BCBlock block);
BCValue bc_insn_jump_if(BCFunction function, BCValue cond, BCBlock block);

BCValue bc_insn_return(BCFunction function, BCValue value);