#include "atcc.h"

Type *make_type(TypeKind kind, u32 size, u32 pack) {
    static u32 typeid = 0;
    Type *type = make(Type);
    type->kind = kind;
    type->typeid = typeid ++;
    type->size = size;
    type->pack = pack;
    return type;
}

void fprint_type(FILE *f, Type *type) {
    switch (type->kind) {
        case TYPE_VOID: fprintf(f, "void"); break;
        case TYPE_I8: fprintf(f, "i8"); break;
        case TYPE_U8: fprintf(f, "u8"); break;
        case TYPE_I16: fprintf(f, "i16"); break;
        case TYPE_U16: fprintf(f, "u16"); break;
        case TYPE_I32: fprintf(f, "i32"); break;
        case TYPE_U32: fprintf(f, "u32"); break;
        case TYPE_I64: fprintf(f, "i64"); break;
        case TYPE_U64: fprintf(f, "u64"); break;
        case TYPE_F32: fprintf(f, "f32"); break;
        case TYPE_F64: fprintf(f, "f64"); break;
        case TYPE_STRING: fprintf(f, "string"); break;
        case TYPE_POINTER:
            print_type(type->base_type);
            fprintf(f, "*");
            break;
        default: fprintf(f, "{typeid%d}", type->typeid); break;
    }
}

void print_type(Type *type) { fprint_type(stdout, type); }

bool type_is_integer(Type *type) { return type->kind >= TYPE_I8 && type->kind <= TYPE_U64; }
bool type_is_scalar(Type *type) { return type->kind >= TYPE_I8 && type->kind <= TYPE_FUNCTION; }
bool type_is_arithmetic(Type *type) { return type->kind >= TYPE_I8 && type->kind <= TYPE_F64; }

Type *node_type(ASTNode *node) { return node->conv_type ? node->conv_type : node->base_type; }