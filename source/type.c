#include "atcc.h"
#include "ati/utils.h"

Type *make_type(TypeKind kind, u32 size, u32 pack) {
    static u32 typeid = 0;
    Type *type = make(Type);
    type->kind = kind;
    type->typeid = typeid ++;
    type->size = size;
    type->pack = pack;
    return type;
}

string type_to_string(Type *type) {
    switch (type->kind) {
        case TYPE_VOID: return str("void");
        case TYPE_I8: return str("i8");
        case TYPE_U8: return str("u8");
        case TYPE_I16: return str("i16");
        case TYPE_U16: return str("u16");
        case TYPE_I32: return str("i32");
        case TYPE_U32: return str("u32");
        case TYPE_I64: return str("i64");
        case TYPE_U64: return str("u64");
        case TYPE_F32: return str("f32");
        case TYPE_F64: return str("f64");
        case TYPE_STRING: return str("string");
        case TYPE_POINTER: {
            string base = type_to_string(type->base_type);
            return string_format(str("%.*s*"), strp(base));
        }
        case TYPE_ARRAY: {
            string base = type_to_string(type->array_base);
            if (type->array_is_dynamic)
                return string_format(str("%.*s[*]"), strp(base));
            else if (type->array_size > 0)
                return string_format(str("%.*s[%d]"), strp(base), type->array_size);
            return string_format(str("%.*s[]"), strp(base));
        }
        case TYPE_AGGREGATE: return type->owner->aggregate_name;
        default: return string_format(str("{typeid%d}"), type->typeid);
    }
}

bool type_match(Type *a, Type *b) {
    if (a->kind != b->kind)
        return false;

    if (a->kind >= TYPE_I8 && a->kind <= TYPE_U64)
        return true;

    if (a->typeid == b->typeid)
        return true;

    switch (a->kind) {
        case TYPE_POINTER: return type_match(a->base_type, b->base_type);
        case TYPE_ARRAY: {
            if (a->array_is_dynamic != b->array_is_dynamic)
                return false;
            if (a->array_size != b->array_size)
                return false;
            return type_match(a->array_base, b->array_base);
        }
        case TYPE_FUNCTION: {
            if (a->function_is_variadic != b->function_is_variadic)
                return false;
            if (vector_length(a->function_parameters) != vector_length(b->function_parameters))
                return false;

            for (u32 i = 0; i < vector_length(a->function_parameters); i += 1) {
                Type *param_a = a->function_parameters[i];
                Type *param_b = b->function_parameters[i];
                if (!type_match(param_a, param_b))
                    return false;
            }

            return type_match(a->function_return_type, b->function_return_type);
        }
        case TYPE_AGGREGATE: return a->owner == b->owner;
        case TYPE_STRING: return true;
        default: break;
    }

    panic("ICE: Unhandled type in type_match");
}

bool type_is_integer(Type *type) { return type->kind >= TYPE_I8 && type->kind <= TYPE_U64; }
bool type_is_scalar(Type *type) { return type->kind >= TYPE_I8 && type->kind <= TYPE_FUNCTION; }
bool type_is_arithmetic(Type *type) { return type->kind >= TYPE_I8 && type->kind <= TYPE_F64; }

Type *node_type(ASTNode *node) { return node->conv_type ? node->conv_type : node->base_type; }