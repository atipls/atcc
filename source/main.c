#include "atcc.h"
#include "ati/table.h"
#include "ati/utils.h"
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

    SemanticContext *context = sema_initialize();

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

        if (!sema_register_program(context, program)) {
            fprintf(stderr, "Registering declarations failed: \n");
            print_semantic_errors(context->errors);
            return 1;
        }
    }

    if (!sema_analyze(context)) {
        fprintf(stderr, "Semantic analysis failed: \n");
        print_semantic_errors(context->errors);
        return 1;
    }

    printf("Global scope has %d entries\n", context->global->entries.length);
    write_program_dot(context->programs[0], "hello.dot");


    return 0;
}
