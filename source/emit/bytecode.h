#pragma once

#include "ati/basic.h"
#include "ati/string.h"

typedef struct SBCValue *BCValue;
typedef struct SBCType *BCType;
typedef struct SBCCode *BCCode;
typedef struct SBCBlock *BCBlock;
typedef struct SBCFunction *BCFunction;
typedef struct SBCContext *BCContext;

typedef enum {
    BC_VALUE_IS_CONSTANT = (1 << 1),
    BC_VALUE_IS_PARAMETER = (1 << 2),
    BC_VALUE_IS_TEMPORARY = (1 << 3),
    BC_VALUE_IS_ON_STACK = (1 << 4),
    BC_VALUE_IS_GLOBAL = (1 << 5),
    BC_VALUE_IS_FUNCTION = (1 << 6),
} BCValueFlags;

struct SBCValue {
    BCValueFlags flags;
    BCType type;
    union {
        u64 storage;
        i64 istorage;
        f64 floating;
        string string;
    };
};

BCValue bc_value_make(BCFunction function, BCType type);

BCValue bc_value_make_consti(BCType type, u64 value);
BCValue bc_value_make_constf(BCType type, f64 value);

BCValue bc_value_get_parameter(BCFunction function, u32 index);

typedef enum {
    BC_TYPE_BASE,
    BC_TYPE_POINTER,
    BC_TYPE_ARRAY,
    BC_TYPE_FUNCTION,
    BC_TYPE_AGGREGATE,
} BCTypeKind;

typedef struct {
    BCType type;
    string name;
    u32 offset;
} BCAggregate;

struct SBCType {
    BCTypeKind kind;
    u32 size;
    u32 alignment;
    bool is_signed;
    bool is_floating;
    u32 emit_index;

    union {
        BCType base;

        struct {
            BCType element;
            BCValue count;
            bool is_dynamic;
        };

        struct {
            BCAggregate *members;
            u32 num_members;
            string name;
        };

        struct {
            BCType result;
            BCType *params;
            u32 num_params;
        };
    };
};

extern BCType bc_type_void;
extern BCType bc_type_i8, bc_type_u8;
extern BCType bc_type_i16, bc_type_u16;
extern BCType bc_type_i32, bc_type_u32;
extern BCType bc_type_i64, bc_type_u64;
extern BCType bc_type_f32, bc_type_f64;

BCType bc_type_pointer(BCType type);
BCType bc_type_array(BCContext context, BCType type, BCValue size, bool is_dynamic);
BCType bc_type_function(BCType result, BCType *params, u32 num_params);
BCType bc_type_aggregate(BCContext context, string name);
void bc_type_aggregate_set_body(BCType aggregate, BCAggregate *members, u32 num_members);

bool bc_type_is_integer(BCType type);

typedef enum {
    BC_OP_NOP,

    BC_OP_LOAD,
    BC_OP_STORE,

    BC_OP_GET_FIELD,
    BC_OP_GET_INDEX,

    BC_OP_ADD,
    BC_OP_SUB,
    BC_OP_MUL,
    BC_OP_DIV,
    BC_OP_MOD,
    BC_OP_NEG,
    BC_OP_NOT,
    BC_OP_AND,
    BC_OP_OR,
    BC_OP_XOR,
    BC_OP_SHL,
    BC_OP_SHR,
    BC_OP_EQ,
    BC_OP_NE,
    BC_OP_LT,
    BC_OP_GT,
    BC_OP_LE,
    BC_OP_GE,

    BC_OP_JUMP,
    BC_OP_JUMP_IF,

    BC_OP_CALL,
    BC_OP_RETURN,

    BC_OP_CAST_BITWISE,
    BC_OP_CAST_INT_TO_PTR,
    BC_OP_CAST_PTR_TO_INT,
    BC_OP_CAST_INT_TRUNC,
    BC_OP_CAST_INT_ZEXT,
    BC_OP_CAST_INT_SEXT,
    BC_OP_CAST_FP_EXTEND,
    BC_OP_CAST_FP_TRUNC,
    BC_OP_CAST_FP_TO_SINT,
    BC_OP_CAST_FP_TO_UINT,
    BC_OP_CAST_SINT_TO_FP,
    BC_OP_CAST_UINT_TO_FP,
} BCOpcode;

struct SBCCode {
    BCBlock block;
    BCOpcode opcode;
    u32 flags;

    union {
        struct {
            BCValue regA;
            BCValue regB;
            BCValue regD;
        };
        struct {
            BCValue regC;
            BCBlock bbT;
            BCBlock bbF;
        };
        struct {
            BCValue target;
            BCValue result;
            BCValue *args;
            u32 num_args;
        };
    };
};

struct SBCBlock {
    u64 serial;
    BCBlock prev, next;

    BCCode *code;
    BCValue input;
};

struct SBCFunction {
    BCType signature;
    string name;

    bool is_extern;
    bool is_variadic;

    BCBlock first_block;
    BCBlock last_block;

    BCBlock current_block;

    BCValue params;
    BCValue *locals;
    u32 last_temporary;
    u32 last_block_serial;
    u32 stack_size;
};

struct SBCContext {
    BCType *arrays;
    BCType *aggregates;
    BCFunction *functions;
    u32 global_size;
};

BCContext bc_context_initialize(void);

BCFunction bc_function_create(BCContext context, BCType signature, string name);
BCBlock bc_function_set_block(BCFunction function, BCBlock block);
BCBlock bc_function_get_block(BCFunction function);
BCValue bc_function_define(BCFunction function, BCType type);

BCBlock bc_block_make(BCFunction function);
bool bc_block_is_terminated(BCBlock block);

BCCode bc_insn_make(BCBlock block);

BCValue bc_insn_nop(BCFunction function);

BCValue bc_insn_load(BCFunction function, BCValue source);
BCValue bc_insn_store(BCFunction function, BCValue dest, BCValue source);
BCValue bc_insn_get_field(BCFunction function, BCValue source, BCType type, u64 index);
BCValue bc_insn_get_index(BCFunction function, BCValue source, BCType type, BCValue index);

BCValue bc_insn_add(BCFunction function, BCValue arg1, BCValue arg2);
BCValue bc_insn_sub(BCFunction function, BCValue arg1, BCValue arg2);
BCValue bc_insn_mul(BCFunction function, BCValue arg1, BCValue arg2);
BCValue bc_insn_div(BCFunction function, BCValue arg1, BCValue arg2);
BCValue bc_insn_mod(BCFunction function, BCValue arg1, BCValue arg2);
BCValue bc_insn_neg(BCFunction function, BCValue arg1, BCValue arg2);
BCValue bc_insn_not(BCFunction function, BCValue arg1, BCValue arg2);
BCValue bc_insn_and(BCFunction function, BCValue arg1, BCValue arg2);
BCValue bc_insn_or(BCFunction function, BCValue arg1, BCValue arg2);
BCValue bc_insn_xor(BCFunction function, BCValue arg1, BCValue arg2);
BCValue bc_insn_shl(BCFunction function, BCValue arg1, BCValue arg2);
BCValue bc_insn_shr(BCFunction function, BCValue arg1, BCValue arg2);
BCValue bc_insn_eq(BCFunction function, BCValue arg1, BCValue arg2);
BCValue bc_insn_ne(BCFunction function, BCValue arg1, BCValue arg2);
BCValue bc_insn_lt(BCFunction function, BCValue arg1, BCValue arg2);
BCValue bc_insn_gt(BCFunction function, BCValue arg1, BCValue arg2);
BCValue bc_insn_le(BCFunction function, BCValue arg1, BCValue arg2);
BCValue bc_insn_ge(BCFunction function, BCValue arg1, BCValue arg2);

BCCode bc_insn_jump(BCFunction function, BCBlock block);
BCCode bc_insn_jump_if(BCFunction function, BCValue cond, BCBlock block_true, BCBlock block_false);

BCValue bc_insn_call(BCFunction function, BCValue target, BCValue *args, u32 num_args);
BCValue bc_insn_return(BCFunction function, BCValue value);

BCValue bc_insn_cast(BCFunction function, BCOpcode opcode, BCValue source, BCType target);

void bc_dump_function(BCFunction function, FILE *f);

typedef enum BCObjectKind {
    BC_OBJECT_KIND_LINUX,
    BC_OBJECT_KIND_WINDOWS,
    BC_OBJECT_KIND_MACOS,
} BCObjectKind;

typedef struct BCSection {
    string name;
    u8 *data;
    u64 size;
} BCSection;

typedef struct BCObject {
    BCObjectKind kind;
    BCSection *sections;
    u32 num_sections;
    void *platform;
} BCObject;

// Binary codegen helpers
typedef struct BCBuffer {
    u8 *data;
    u64 size;
    u64 capacity;
} BCBuffer;

BCBuffer *bc_buffer_create(u64 initial_capacity);
void bc_buffer_destroy(BCBuffer *buffer);

u64 bc_emit_u8(BCBuffer *buffer, u8 value);
u64 bc_emit_u16(BCBuffer *buffer, u16 value);
u64 bc_emit_u32(BCBuffer *buffer, u32 value);
u64 bc_emit_u64(BCBuffer *buffer, u64 value);
u64 bc_emit_f32(BCBuffer *buffer, f32 value);
u64 bc_emit_f64(BCBuffer *buffer, f64 value);
u64 bc_emit_data(BCBuffer *buffer, u8 *data, u64 size);

void bc_patch_u8(BCBuffer *buffer, u64 offset, u8 value);
void bc_patch_u16(BCBuffer *buffer, u64 offset, u16 value);
void bc_patch_u32(BCBuffer *buffer, u64 offset, u32 value);
void bc_patch_u64(BCBuffer *buffer, u64 offset, u64 value);
void bc_patch_f32(BCBuffer *buffer, u64 offset, f32 value);
void bc_patch_f64(BCBuffer *buffer, u64 offset, f64 value);


bool bc_generate_amd64(BCContext context, BCObjectKind object_kind, FILE *f);
bool bc_generate_arm64(BCContext context, BCObjectKind object_kind, FILE *f);
bool bc_generate_source(BCContext context, FILE *f);

void bc_register_utest(void);
