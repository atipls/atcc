#include "atcc.h"
#include "ati/table.h"
#include "ati/utils.h"
#include "emit/bytecode.h"
#include <assert.h>

struct {
    string *inputs;
    string output;
} options;

bool parse_options(i32 argc, cstring argv[]) {
    for (i32 i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == 'o') {
                if (i + 1 >= argc)
                    return false;
                options.output = string_from_cstring(argv[++i]);
                continue;
            }

            fprintf(stderr, "Unknown option: '%s'\n", argv[i]);
            return false;
        }

        vector_push(options.inputs, string_from_cstring(argv[i]));
    }

    return true;
}

static void print_semantic_errors(SemanticError *errors) {
    vector_foreach(SemanticError, error, errors) {
        fprintf(stderr, "%.*s:%d:%d: Error: %.*s\n",
                (i32) error->location.file.length, error->location.file.data,
                error->location.line, error->location.column,
                (i32) error->description.length, error->description.data);
    }
}

i32 main(i32 argc, cstring argv[]) {
    options.output = str("generated");
#if 0
    if (!parse_options(argc, argv)) {
        fprintf(stderr, "Usage: atcc [options] [files]\n");
        return 1;
    }
#else
    (void) argc;
    (void) argv;
    vector_push(options.inputs, str("sample/simple.aa"));
#endif

    SemanticContext *sema_context = sema_initialize();

#if 0
        do {
        BCContext bc_context = bc_context_initialize();
        BCType bc_main_type = bc_type_function(bc_type_i32, (BCType[]){bc_type_i32, bc_type_i32}, 2);
        BCFunction bc_main = bc_function_create(bc_context, bc_main_type, str("GetGCD"));

        BCValue u, v, t;

        u = bc_value_get_parameter(bc_main, 0);
        v = bc_value_get_parameter(bc_main, 1);
        t = bc_value_make(bc_main, bc_type_i32);

        BCBlock label_p1 = bc_block_make(bc_main);
        BCBlock label_p2 = bc_block_make(bc_main);

        BCValue zero = bc_value_make_consti(bc_main, bc_type_i32, 0);

        bc_function_set_block(bc_main, label_p1);

        BCValue compare_v = bc_insn_eq(bc_main, v, zero);
        bc_insn_jump_if(bc_main, compare_v, label_p2);

        bc_insn_store(bc_main, t, u);
        bc_insn_store(bc_main, u, v);

        BCValue remainder = bc_insn_mod(bc_main, u, v);
        bc_insn_store(bc_main, v, remainder);

        bc_insn_jump(bc_main, label_p1);

        bc_function_set_block(bc_main, label_p2);

        BCBlock label_p3 = bc_block_make(bc_main);
        BCValue compare_u = bc_insn_ge(bc_main, u, zero);
        bc_insn_jump_if(bc_main, compare_u, label_p3);

        BCValue minus_u = bc_insn_sub(bc_main, zero, u);
        bc_insn_return(bc_main, minus_u);

        bc_function_set_block(bc_main, label_p3);
        bc_insn_return(bc_main, u);

        bc_dump_function(bc_main, stdout);

        FILE *file = fopen("generated.c", "wb");
        if (!bc_generate_source(bc_context, file))
            fprintf(stderr, "BC Emit failed.\n");
        fclose(file);

    } while (0);
#endif

    vector_foreach(string, filename, options.inputs) {
        Buffer buffer = read_file(*filename);
        if (!buffer.data) {
            fprintf(stderr, "Failed to read file: %s\n", filename->data);
            return 1;
        }

        Token *tokens = lexer_tokenize(*filename, buffer);
        ASTNode *program = parse_program(tokens);
        assert(program->kind == AST_PROGRAM);
        // write_program_dot(program, "hello.dot");

        if (!sema_register_program(sema_context, program)) {
            fprintf(stderr, "Registering declarations failed: \n");
            print_semantic_errors(sema_context->errors);
            return 1;
        }
    }

    if (!sema_analyze(sema_context)) {
        fprintf(stderr, "Semantic analysis failed: \n");
        print_semantic_errors(sema_context->errors);
        return 1;
    }

    printf("Global scope has %d entries\n", sema_context->global->entries.length);
    write_program_dot(sema_context->programs[0], "hello.dot");

    BuildContext *build_context = build_initialize(sema_context);
    if (!build_bytecode(build_context)) {
        fprintf(stderr, "Building bytecode failed.\n");
        return 1;
    }

    FILE *file = fopen("generated.c", "wb");
    if (!bc_generate_source(build_context->bc, file))
        fprintf(stderr, "BC Emit failed.\n");
    fclose(file);

    return 0;
}
