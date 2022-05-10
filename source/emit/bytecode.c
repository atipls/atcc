#include "bytecode.h"
#include "atcc.h"

#define BASE_TYPE(_size, _is_signed, ...) \
    &(struct BCType) { .kind = BC_TYPE_BASE, .size = _size, .alignment = _size, .is_signed = _is_signed, __VA_ARGS__ }


BCType bc_type_void = BASE_TYPE(0, false);
BCType bc_type_i8 = BASE_TYPE(1, true), bc_type_u8 = BASE_TYPE(1, false);
BCType bc_type_i16 = BASE_TYPE(2, true), bc_type_u16 = BASE_TYPE(2, false);
BCType bc_type_i32 = BASE_TYPE(4, true), bc_type_u32 = BASE_TYPE(4, false);
BCType bc_type_i64 = BASE_TYPE(8, true), bc_type_u64 = BASE_TYPE(8, false);
BCType bc_type_f32 = BASE_TYPE(4, true, .is_floating = true),
       bc_type_f64 = BASE_TYPE(8, true, .is_floating = true);

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

BCType bc_type_function(BCType result, BCType* params, u32 num_params) {
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