#include "atcc.h"
#include "ati/table.h"
#include "ati/utest.h"
#include "ati/utils.h"
#include "emit/bytecode.h"
#include <assert.h>
#include <string.h>

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

int utest_test_1() {
    return UTEST_PASS;
}

int utest_test_2() {
    return UTEST_FAIL;
}

int utest_test_3() {
    *(i32 volatile *) 0 = 0;

    return UTEST_PASS;
}

UTest utests_main[] = {
    { str("test_1"), utest_test_1 },
    { str("test_2"), utest_test_2 },
    { str("test_3"), utest_test_3 },
};

i32 main(i32 argc, cstring argv[]) {
    utest_register(str("main"), utests_main, array_length(utests_main));

    if (argc > 1 && strcmp(argv[1], "--utest") == 0) {
        utest_run();
        return 0;
    }

    options.output = str("generated");
#if 1//-1
    if (!parse_options(argc, argv)) {
        fprintf(stderr, "Usage: atcc [options] <files>\n");
        return 1;
    }
#else
    (void) argc;
    (void) argv;
    vector_push(options.inputs, str("tests/preload.aa"));
    vector_push(options.inputs, str("tests/cases/05-array.aa"));
#endif
    SemanticContext *sema_context = sema_initialize();

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

    if (!string_table_get(&sema_context->global->entries, str("Main"))) {
        fprintf(stderr, "error: Main function not found.\n");
        return 1;
    }

    debug printf("Global scope has %d entries\n", sema_context->global->entries.length);
    debug write_program_dot(sema_context->programs[0], "hello.dot");

    BuildContext *build_context = build_initialize(sema_context);
    if (!build_bytecode(build_context)) {
        fprintf(stderr, "Building bytecode failed.\n");
        return 1;
    }

    FILE *file = fopen("generated.c", "wb");
    if (!bc_generate_source(build_context->bc, file))
        fprintf(stderr, "BC Emit failed.\n");
    fclose(file);

    file = fopen("output.bin", "wb");
    if (!bc_generate_arm64(build_context->bc, BC_OBJECT_KIND_MACOS, file))
        fprintf(stderr, "BC Emit failed.\n");
    fclose(file);

    return 0;
}
