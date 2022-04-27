#pragma once
#include "ati/basic.h"

typedef struct BCValue *BCValue;
typedef struct BCType *BCType;
typedef struct BCCode *BCCode;
typedef struct BCBlock *BCBlock;

typedef enum {
    BC_VALUE_IS_CONSTANT = (1 << 1),
    BC_VALUE_IS_VOLATILE = (1 << 2),
    BC_VALUE_IS_REGISTER = (1 << 3),
    BC_VALUE_IS_ADDRESSED = (1 << 4),
    BC_VALUE_IS_PARAMETER = (1 << 5),
} BCValueFlags;

struct BCValue {
    BCValueFlags flags;
    BCType *type;
    u64 storage;
};

struct BCType {
    u32 size;
    u32 alignment;
    bool is_signed;
    bool is_floating;
    BCType *parent;
};

extern BCType bc_type_void;
extern BCType bc_type_i8, bc_type_u8;
extern BCType bc_type_i16, bc_type_u16;
extern BCType bc_type_i32, bc_type_u32;
extern BCType bc_type_i64, bc_type_u64;
extern BCType bc_type_f32, bc_type_f64;

BCType bc_type_pointer(BCType type);
BCType bc_type_array(BCType type, u32 size);
BCType bc_type_struct(BCType *types, u32 size);

typedef enum {
    BC_OP_NOP,

    BC_OP_LOAD,
    BC_OP_LOAD_ADDRESS,
    BC_OP_STORE,

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
} BCOpcode;

struct BCCode {
    BCBlock *block;
};

struct BCBlock {
};