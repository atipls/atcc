#include <assert.h>
#include "atcc.h"
#include "ati/utils.h"

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
    vector_push(options.inputs, str("sample/hello.aa"));
#endif

    vector_foreach(string, filename, options.inputs) {
        Buffer buffer = read_file(*filename);
        if (!buffer.data) {
            fprintf(stderr, "Failed to read file: %s\n", filename->data);
            return 1;
        }

        Token *tokens = lexer_tokenize(*filename, buffer);
        ASTNode *tree = parse_program(tokens);
        assert(tree->kind == AST_PROGRAM);

        (void) tree;
    }

    return 0;
}
