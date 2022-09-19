#pragma once

#include "ati/basic.h"
#include "ati/string.h"
#include "ati/table.h"
#include "emit/bytecode.h"

#include <assert.h>

typedef struct {
    string file;
    int line;
    int column;
} Location;

typedef enum {
    TOKEN_NONE,
    TOKEN_EOF,
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_CHAR,

    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,
    TOKEN_AMPERSAND,
    TOKEN_PIPE,
    TOKEN_CARET,
    TOKEN_TILDE,
    TOKEN_EQUAL,
    TOKEN_EXCLAMATION,
    TOKEN_COLON,
    TOKEN_LESS,
    TOKEN_GREATER,
    TOKEN_DOT,
    TOKEN_COMMA,
    TOKEN_QUESTION,
    TOKEN_OPEN_PAREN,
    TOKEN_CLOSE_PAREN,
    TOKEN_OPEN_BRACKET,
    TOKEN_CLOSE_BRACKET,
    TOKEN_OPEN_BRACE,
    TOKEN_CLOSE_BRACE,
    TOKEN_HASH,
    TOKEN_DOLLAR,
    TOKEN_AT,
    TOKEN_AMPERSAND_AMPERSAND,
    TOKEN_PIPE_PIPE,
    TOKEN_LEFT_SHIFT,
    TOKEN_RIGHT_SHIFT,
    TOKEN_SEMICOLON,
    TOKEN_DOT_DOT,

    TOKEN_PLUS_EQUAL,
    TOKEN_MINUS_EQUAL,
    TOKEN_STAR_EQUAL,
    TOKEN_SLASH_EQUAL,
    TOKEN_PERCENT_EQUAL,
    TOKEN_AMPERSAND_EQUAL,
    TOKEN_PIPE_EQUAL,
    TOKEN_CARET_EQUAL,
    TOKEN_TILDE_EQUAL,
    TOKEN_AMPERSAND_AMPERSAND_EQUAL,
    TOKEN_PIPE_PIPE_EQUAL,
    TOKEN_LEFT_SHIFT_EQUAL,
    TOKEN_RIGHT_SHIFT_EQUAL,

    TOKEN_EQUAL_EQUAL,
    TOKEN_EXCLAMATION_EQUAL,
    TOKEN_COLON_EQUAL,
    TOKEN_LESS_EQUAL,
    TOKEN_GREATER_EQUAL,

    TOKEN_KW_ALIAS,
    TOKEN_KW_ALIGNOF,
    TOKEN_KW_BREAK,
    TOKEN_KW_CASE,
    TOKEN_KW_CAST,
    TOKEN_KW_CONST,
    TOKEN_KW_CONTINUE,
    TOKEN_KW_DEFAULT,
    TOKEN_KW_DO,
    TOKEN_KW_ELSE,
    TOKEN_KW_ENUM,
    TOKEN_KW_FOR,
    TOKEN_KW_FUN,
    TOKEN_KW_IF,
    TOKEN_KW_OFFSETOF,
    TOKEN_KW_RETURN,
    TOKEN_KW_SIZEOF,
    TOKEN_KW_STRUCT,
    TOKEN_KW_SWITCH,
    TOKEN_KW_UNION,
    TOKEN_KW_VAR,
    TOKEN_KW_WHILE,
} TokenKind;

typedef enum {
    TOKEN_FLAG_NONE = 0,
    TOKEN_FLAG_HEX = 1 << 0,
    TOKEN_FLAG_OCT = 1 << 1,
    TOKEN_FLAG_BIN = 1 << 2,
    TOKEN_FLAG_FLOAT = 1 << 3,
} TokenFlag;

typedef struct {
    TokenKind kind;
    TokenFlag flags;
    Location location;
    string value;
} Token;

Token *lexer_tokenize(string filename, Buffer data);

typedef enum {
    AST_NONE,
    AST_ERROR,
    AST_PROGRAM,
    AST_DECLARATION_TYPE_NAME,
    AST_DECLARATION_TYPE_POINTER,
    AST_DECLARATION_TYPE_ARRAY,
    AST_DECLARATION_TYPE_FUNCTION,
    AST_DECLARATION_ALIAS,
    AST_DECLARATION_AGGREGATE,
    AST_DECLARATION_AGGREGATE_FIELD,
    AST_DECLARATION_AGGREGATE_CHILD,
    AST_DECLARATION_VARIABLE,
    AST_DECLARATION_FUNCTION_PARAMETER,
    AST_DECLARATION_FUNCTION,

    AST_STATEMENT_BLOCK,
    AST_STATEMENT_IF,
    AST_STATEMENT_WHILE,
    AST_STATEMENT_DO_WHILE,
    AST_STATEMENT_FOR,
    AST_STATEMENT_SWITCH,
    AST_STATEMENT_SWITCH_CASE,
    AST_STATEMENT_SWITCH_PATTERN,
    AST_STATEMENT_BREAK,
    AST_STATEMENT_CONTINUE,
    AST_STATEMENT_RETURN,
    AST_STATEMENT_INIT,
    AST_STATEMENT_EXPRESSION,
    AST_STATEMENT_ASSIGN,

    AST_EXPRESSION_PAREN,
    AST_EXPRESSION_UNARY,
    AST_EXPRESSION_BINARY,
    AST_EXPRESSION_TERNARY,
    AST_EXPRESSION_LITERAL_NUMBER,
    AST_EXPRESSION_LITERAL_CHAR,
    AST_EXPRESSION_LITERAL_STRING,
    AST_EXPRESSION_IDENTIFIER,
    AST_EXPRESSION_CALL,
    AST_EXPRESSION_FIELD,
    AST_EXPRESSION_INDEX,
    AST_EXPRESSION_CAST,
    AST_EXPRESSION_COMPOUND,
    AST_EXPRESSION_COMPOUND_FIELD,
    AST_EXPRESSION_COMPOUND_FIELD_NAME,
    AST_EXPRESSION_COMPOUND_FIELD_INDEX,
} ASTKind;

typedef struct ASTNode ASTNode;
typedef struct Type Type;

struct ASTNode {
    ASTKind kind;
    Location location;
    Type *base_type;
    Type *conv_type;

    union {
        string value;
        ASTNode *parent;
        ASTNode **declarations;
        ASTNode **statements;

        struct {
            ASTNode *array_base;
            ASTNode *array_size;
            bool array_is_dynamic;
        };

        struct {
            string function_parameter_name;
            ASTNode *function_parameter_type;
        };

        struct {
            string function_name;
            ASTNode **function_parameters;
            ASTNode *function_return_type;
            bool function_is_variadic;
            ASTNode *function_body;
        };

        struct {
            string alias_name;
            ASTNode *alias_type;
        };

        struct {
            string aggregate_name;
            ASTNode *aggregate;
        };

        struct {
            TokenKind aggregate_kind;
            ASTNode **aggregate_items;
        };

        struct {
            string *aggregate_names;
            ASTNode *aggregate_type;
        };

        struct {
            string variable_name;
            ASTNode *variable_type;
            ASTNode *variable_initializer;
            bool variable_is_const;
        };

        struct {
            ASTNode *if_expression;
            ASTNode *if_condition;
            ASTNode *if_true;
            ASTNode *if_false;
        };

        struct {
            ASTNode *while_condition;
            ASTNode *while_body;
        };

        struct {
            ASTNode *for_initializer;
            ASTNode *for_condition;
            ASTNode *for_increment;
            ASTNode *for_body;
        };

        struct {
            ASTNode *switch_expression;
            ASTNode **switch_cases;
        };

        struct {
            ASTNode **switch_case_patterns;
            ASTNode *switch_case_body;
            bool switch_case_is_default;
        };

        struct {
            ASTNode *switch_pattern_start;
            ASTNode *switch_pattern_end;
        };

        struct {
            string init_name;
            ASTNode *init_type;
            ASTNode *init_value;
            bool init_is_nothing;
        };

        struct {
            TokenKind assign_operator;
            ASTNode *assign_target;
            ASTNode *assign_value;
        };

        struct {
            TokenKind unary_operator;
            ASTNode *unary_target;
        };

        struct {
            TokenKind binary_operator;
            ASTNode *binary_left;
            ASTNode *binary_right;
        };

        struct {
            ASTNode *ternary_condition;
            ASTNode *ternary_true;
            ASTNode *ternary_false;
        };

        struct {
            string literal_value;
            TokenFlag literal_flags;
            union {
                u64 literal_as_u64;
                f64 literal_as_f64;
            };
        };

        struct {
            ASTNode *call_target;
            ASTNode **call_arguments;
        };

        struct {
            ASTNode *field_target;
            string field_name;
        };

        struct {
            ASTNode *index_target;
            ASTNode *index_index;
        };

        struct {
            ASTNode *cast_type;
            ASTNode *cast_target;
        };

        struct {
            ASTNode *compound_type;
            ASTNode **compound_fields;
        };

        struct {
            string compound_field_name;
            ASTNode *compound_field_index;
            ASTNode *compound_field_target;
        };
    };
};

ASTNode *parse_program(Token *tokens);
void write_program_dot(ASTNode *node, cstring filename);

typedef enum {
    TYPE_NONE,
    TYPE_VOID,
    TYPE_I8,
    TYPE_U8,
    TYPE_I16,
    TYPE_U16,
    TYPE_I32,
    TYPE_U32,
    TYPE_I64,
    TYPE_U64,
    TYPE_F32,
    TYPE_F64,
    TYPE_POINTER,
    TYPE_FUNCTION,
    TYPE_STRING,
    TYPE_ARRAY,
    TYPE_AGGREGATE,
} TypeKind;

typedef struct TypeField {
    string name;
    Type *type;
    bool address_only;
} TypeField;

struct Type {
    TypeKind kind;
    u32 typeid;
    u32 size;
    u32 pack;

    union {
        Type *base_type;

        struct {
            Type **function_parameters;
            Type *function_return_type;
            bool function_is_variadic;
        };

        struct {
            Type *array_base;
            u32 array_size;
            bool array_is_dynamic;
        };

        struct {
            ASTNode *owner;
            TypeField *fields;
            bool is_complete;
        };

    };
};

Type *make_type(TypeKind kind, u32 size, u32 pack);
void print_type(Type *type);

bool type_is_integer(Type *type);
bool type_is_scalar(Type *type);
bool type_is_arithmetic(Type *type);

Type *node_type(ASTNode *node);

typedef enum {
    SEMA_ENTRY_NONE,
    SEMA_ENTRY_TYPE,
    SEMA_ENTRY_VARIABLE,
    SEMA_ENTRY_FUNCTION,
} SemanticEntryKind;

typedef enum {
    SEMA_STATE_UNRESOLVED,
    SEMA_STATE_RESOLVING,
    SEMA_STATE_RESOLVED,
} SemanticEntryState;

typedef struct {
    SemanticEntryKind kind;
    SemanticEntryState state;
    ASTNode *node;
    Type *type;
} SemanticEntry;

typedef struct SemanticScope SemanticScope;
struct SemanticScope {
    StringTable entries;
    bool can_break;
    bool can_continue;
    SemanticScope *parent;
};

typedef struct {
    Location location;
    string description;
} SemanticError;

typedef struct {
    ASTNode **programs;
    SemanticScope *global;
    SemanticScope *scope;
    SemanticError *errors;

    PointerTable array_types;
    PointerTable pointer_types;

    Type *type_void;
    Type *type_i8;
    Type *type_u8;
    Type *type_i16;
    Type *type_u16;
    Type *type_i32;
    Type *type_u32;
    Type *type_i64;
    Type *type_u64;
    Type *type_f32;
    Type *type_f64;
    Type *type_string;
} SemanticContext;

SemanticContext *sema_initialize();
bool sema_register_program(SemanticContext *context, ASTNode *program);
bool sema_analyze(SemanticContext *context);

typedef struct {
    SemanticContext *sema;
    BCContext bc;

    StringTable aggregates;
    StringTable functions;
    StringTable globals;
    StringTable locals;

    PointerTable types;

    BCFunction function;
    BCFunction initializer;

    BCBlock break_target;
    BCBlock continue_target;
} BuildContext;

BuildContext *build_initialize(SemanticContext *sema);
bool build_bytecode(BuildContext *context);