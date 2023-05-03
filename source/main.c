#include "atcc.h"
#include "ati/config.h"
#include "ati/table.h"
#include "ati/utest.h"
#include "ati/utils.h"
#include "emit/bytecode.h"
#include <assert.h>
#include <string.h>

#define PRELOAD_INCLUDED
#include "preload.c"
#undef PRELOAD_INCLUDED

struct {
    string *inputs;
    string output;
    string backend;

    bool write_dot;
} settings;

VerboseFlags verbose = 0;

bool parse_options(i32 argc, cstring argv[]) {
    for (i32 i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == 'o') {
                if (i + 1 >= argc)
                    return false;
                settings.output = string_from_cstring(argv[++i]);
                continue;
            }

            if (argv[i][1] == 'b') {
                if (i + 1 >= argc)
                    return false;
                settings.backend = string_from_cstring(argv[++i]);
                continue;
            }

            if (argv[i][1] == 'd') {
                if (i + 1 >= argc)
                    return false;
                settings.write_dot = true;
                continue;
            }

            if (argv[i][1] == 'v') {
                if (i + 1 >= argc)
                    return false;
                verbose = atoi(argv[++i]);
                continue;
            }

            fprintf(stderr, "Unknown option: '%s'\n", argv[i]);
            return false;
        }

        vector_push(settings.inputs, string_from_cstring(argv[i]));
    }

    return true;
}

static void print_semantic_errors(SemanticError *errors) {
    vector_foreach(SemanticError, error, errors) {
        fprintf(stderr, "\x1b[94m%.*s:%d:%d:\x1b[0m ",
                strp(error->location.file), error->location.line, error->location.column);

        fprintf(stderr, "\033[31merror: \033[0m%.*s\n", strp(error->description));
    }
}

static void print_help() {
    fprintf(stderr, "Usage: atcc [options] <input files>\n");
    fprintf(stderr, "       atcc --config <config file>\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -o <file> Set output file\n");
    fprintf(stderr, "  -b <b>    Set backend\n");
    fprintf(stderr, "  -d        Write dot files\n");
    fprintf(stderr, "  -v        Verbose output\n");
}

static i32 compiler_load_preload(SemanticContext *sema_context) {
    Token *tokens = lexer_tokenize(str("preload.aa"), (Buffer){(i8 *) preload_source, preload_source_len});
    ASTNode *program = parse_program(tokens);

    if (!sema_register_program(sema_context, program)) {
        fprintf(stderr, "Registering declarations for preload failed: \n");
        print_semantic_errors(sema_context->errors);
        return 0;
    }

    return 1;
}

static i32 compiler_main(string *inputs, string output, string backend, bool write_dot) {
    SemanticContext *sema_context = sema_initialize();

    if (!compiler_load_preload(sema_context)) {
        fprintf(stderr, "Failed to load preload\n");
        return 1;
    }

    vector_foreach(string, filename, inputs) {
        Buffer buffer = read_file(*filename);
        if (!buffer.data) {
            fprintf(stderr, "Failed to read file: %.*s\n", strp(*filename));
            return 1;
        }

        Token *tokens = lexer_tokenize(*filename, buffer);
        ASTNode *program = parse_program(tokens);
        assert(program->kind == AST_PROGRAM);

        if (write_dot)
            write_program_dot(program, "hello.dot");

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

    if (vector_len(sema_context->errors) > 0) {
        fprintf(stderr, "\033[31mThe following errors were found during semantic analysis:\033[0m\n");
        print_semantic_errors(sema_context->errors);
        fprintf(stderr, "    \033[36m-> stopping compilation.\033[0m\n");
        fflush(stderr);
        return 1;
    }

    if (!string_table_get(&sema_context->global->entries, str("Main"))) {
        fprintf(stderr, "error: Main function not found.\n");
        return 1;
    }

    BuildContext *build_context = build_initialize(sema_context);
    if (!build_bytecode(build_context)) {
        fprintf(stderr, "Building bytecode failed.\n");
        return 1;
    }

    if (string_match(backend, str("c"))) {
        string outpath = string_format(str("%.*s.c"), strp(output));

        FILE *file = fopen(string_to_cstring(outpath), "wb");
        if (!bc_generate_source(build_context->bc, file))
            fprintf(stderr, "Generating C source failed.\n");
        fclose(file);
    } else if (string_match(backend, str("arm64"))) {
        string outpath = string_format(str("%.*s.bin"), strp(output));

        FILE *file = fopen(string_to_cstring(outpath), "wb");
        if (!bc_generate_arm64(build_context->bc, BC_OBJECT_KIND_MACOS, file))
           fprintf(stderr, "Generating ARM64 failed.\n");
        fclose(file);
    } else {
        fprintf(stderr, "Unknown backend: %.*s\n", strp(backend));
        return 1;
    }

    return 0;
}

static i32 config_main(cstring config) {
    string config_path = string_from_cstring(config);
    Buffer config_buffer = read_file(config_path);

    if (!config_buffer.data) {
        fprintf(stderr, "Failed to read config file: %.*s\n", strp(config_path));
        return 1;
    }

    option *options = options_parse(config_path, config_buffer);
    if (!options) {
        fprintf(stderr, "Failed to parse config file: %.*s\n", strp(config_path));
        return 1;
    }

    entry *input_entries;
    string output;
    string backend;

    string write_dot_string;
    string verbose_string;

    bool write_dot = false;

    if (!options_get_list(options, str("files"), &input_entries))
        return 1;

    options_get_default(options, str("output"), &output, str("generated"));
    options_get_default(options, str("backend"), &backend, str("c"));

    options_get_default(options, str("dot"), &write_dot_string, str("false"));
    options_get_default(options, str("verbose"), &verbose_string, str("0"));

    write_dot = string_match(write_dot_string, str("true"));
    verbose = atoi(string_to_cstring(verbose_string));

    string *inputs = null;
    vector_foreach(entry, entry, input_entries) {
        if (entry->tag.length)
            continue;
        vector_push(inputs, entry->value);
    }

    return compiler_main(inputs, output, backend, write_dot);
}

static i32 standalone_main(i32 argc, cstring argv[]) {
    settings.output = str("generated");
    settings.backend = str("c");

    if (!parse_options(argc, argv)) {
        print_help();
        return 1;
    }

    if (!settings.inputs) {
        print_help();
        return 1;
    }

    return compiler_main(settings.inputs, settings.output, settings.backend, settings.write_dot);
}

i32 main(i32 argc, cstring argv[]) {
    if (argc > 1 && strcmp(argv[1], "--utest") == 0) {
        ati_register_utest();
        bc_register_utest();

        return utest_run();
    }


    if (argc > 2 && string_match_cstring(str("--config"), argv[1]))
        return config_main(argv[2]);

    return standalone_main(argc, argv);
}
