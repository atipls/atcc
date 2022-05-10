#pragma once

#include <ati/basic.h>
#include <ati/string.h>

typedef struct BCValue *BCValue;
typedef struct BCType *BCType;
typedef struct BCCode *BCCode;
typedef struct BCBlock *BCBlock;
typedef struct BCFunction *BCFunction;
typedef struct BCContext *BCContext;

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

struct BCType {
    BCTypeKind kind;
    u32 size;
    u32 alignment;
    bool is_signed;
    bool is_floating;
    BCValue count;
    BCType subtype;
    BCAggregate *members;
    BCType *params;
    u32 num_params;
};

extern BCType bc_type_void;
extern BCType bc_type_i8, bc_type_u8;
extern BCType bc_type_i16, bc_type_u16;
extern BCType bc_type_i32, bc_type_u32;
extern BCType bc_type_i64, bc_type_u64;
extern BCType bc_type_f32, bc_type_f64;

BCType bc_type_pointer(BCType type);
BCType bc_type_array(BCType type, BCValue size);
BCType bc_type_function(BCType result, BCType *params, u32 num_params);
BCType bc_type_aggregate(BCType type, string name, u32 offset);

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
    BCBlock block;
    BCOpcode opcode;
    u32 flags;

    BCValue dest;
    BCValue regA;
    BCValue regB;
};

struct BCBlock {
    BCCode *code;
};

struct BCFunction {
    BCType signature;
    string name;

    BCBlock *blocks;
};

struct BCContext {
    BCFunction *functions;
};

BCContext bc_context_initialize();

BCFunction bc_function_create(BCContext context, BCType signature, string name);
