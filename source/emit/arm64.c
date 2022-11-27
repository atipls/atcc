#include "bytecode.h"

static BCBuffer *buffer = null;

bool bc_generate_arm64(BCContext context, BCObjectKind object_kind, FILE *f) {
    buffer = bc_buffer_create(512);

    // Basic hello world for macOS arm64:
    // mov x0, #1
    // mov x1, hello_world
    // mov x2, #13
    // mov x16, #4
    // svc #0x80
    //
    // mov x0, #0
    // mov x16, #1
    // svc #0x80
    //
    // hello_world:
    // .ascii "Hello, world!"

    bc_emit_u32(buffer, 0xd2800020); // mov x0, #1
    bc_emit_u32(buffer, 0xd2800090); // mov x1, hello_world
    bc_emit_u32(buffer, 0xd4001001); // mov x2, #13
    bc_emit_u32(buffer, 0xd2800000); // mov x16, #4

    bc_emit_u32(buffer, 0xd2800030); // svc #0x80
    bc_emit_u32(buffer, 0xd4001001); // mov x0, #0

    bc_emit_u32(buffer, 0x6c6c6548); // .ascii "Hello, world!"
    bc_emit_u32(buffer, 0x77202c6f); // .ascii "Hello, world!"
    bc_emit_u32(buffer, 0x646c726f); // .ascii "Hello, world!"
    bc_emit_u32(buffer, 0x00000021); // .ascii "Hello, world!"

    for (u64 i = 0; i < buffer->size; i += 4) {
        u32 instruction = *(u32 *) (buffer->data + i);
        printf("%08x\n", instruction);
    }

    fwrite(buffer->data, 1, buffer->size, f);

    return true;
}