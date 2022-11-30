#include "bytecode.h"

typedef enum AArch64Reg {
    X0,
    X1,
    X2,
    X3,
    X4,
    X5,
    X6,
    X7,
    X8,
    X9,
    X10,
    X11,
    X12,
    X13,
    X14,
    X15,
    X16,
    X17,
    X18,
    X19,
    X20,
    X21,
    X22,
    X23,
    X24,
    X25,
    X26,
    X27,
    X28,
    X29,
    X30,
    XZR,
    SP,
    FP,
    LR,
    PC,

    W0 = X0,
    W1 = X1,
    W2 = X2,
    W3 = X3,
    W4 = X4,
    W5 = X5,
    W6 = X6,
    W7 = X7,
    W8 = X8,
    W9 = X9,
    W10 = X10,
    W11 = X11,
    W12 = X12,
    W13 = X13,
    W14 = X14,
    W15 = X15,
    W16 = X16,
    W17 = X17,
    W18 = X18,
    W19 = X19,
    W20 = X20,
    W21 = X21,
    W22 = X22,
    W23 = X23,
    W24 = X24,
    W25 = X25,
    W26 = X26,
    W27 = X27,
    W28 = X28,
    W29 = X29,
    W30 = X30,
    WZR = XZR
} AArch64Reg;


static BCBuffer *buffer = null;

static void arm64_emit_mov64(AArch64Reg dst, AArch64Reg src) {
    bc_emit_u32(buffer, 0xAA0003E0 | src << 16 | dst);
}

static void arm64_emit_mov32(AArch64Reg dst, AArch64Reg src) {
    bc_emit_u32(buffer, 0x2A0003E0 | src << 16 | dst);
}

static void arm32_emit_load32(AArch64Reg dst, u32 value) {
    bc_emit_u32(buffer, 0x58000000 | (value & 0xFFFF) << 5 | dst);
    bc_emit_u32(buffer, 0x58000000 | (value >> 16) << 5 | dst);
}

static void arm64_emit_load64(AArch64Reg dst, u64 value) {
    bc_emit_u32(buffer, 0xD2800000 | ((value & 0xFFFF) << 5) | dst);
    if (value & 0xFFFF0000) bc_emit_u32(buffer, 0xF2A00000 | ((value & 0xFFFF0000) >> 11) | dst);
    if (value & 0xFFFF00000000) bc_emit_u32(buffer, 0xF2C00000 | ((value & 0xFFFF00000000) >> 27) | dst);
    if (value & 0xFFFF000000000000) bc_emit_u32(buffer, 0xF2E00000 | ((value & 0xFFFF000000000000) >> 43) | dst);
}

static void arm64_emit_svc(u16 value) {
    bc_emit_u32(buffer, 0xD4000001 | value << 5);
}

bool bc_generate_arm64(BCContext context, BCObjectKind object_kind, FILE *f) {
    buffer = bc_buffer_create(512);

    // D2 - mov wx
    // 72 - movk wx
    // 52 - movz wx

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

    for (int i = 0; i < 31; i++) {
        arm64_emit_load64(i, 0x1234567890ABCDEF + i);
    }

    arm64_emit_load64(X0, 0x1234567890ABCDEF);

    for (int i = 0; i < 31; i++) {
        for (int j = 0; j < 31; j++) {
            arm64_emit_mov64(i, j);
        }
    }

    for (int i = 0; i < 31; i++) {
        for (int j = 0; j < 31; j++) {
            arm64_emit_mov32(i, j);
        }
    }

    arm64_emit_load64(X0, 1);
    u64 hello_world = buffer->size;
    arm64_emit_load64(X1, 0);
    arm64_emit_load64(X2, 13);
    arm64_emit_load64(X16, 4);
    arm64_emit_svc(0x80);

    arm64_emit_load64(X0, 0);
    arm64_emit_load64(X16, 1);
    arm64_emit_svc(0x80);

    u64 hello_world_data = bc_emit_data(buffer, (u8 *) "Hello, world!", 14);

    bc_patch_u32(buffer, hello_world, 0xD2800000 | ((hello_world_data & 0xFFFF) << 5) | X1);

    /*
    bc_emit_u32(buffer, 0xd2800020);// mov x0, #1
    bc_emit_u32(buffer, 0xd2800090);// mov x1, hello_world
    bc_emit_u32(buffer, 0xd4001001);// mov x2, #13
    bc_emit_u32(buffer, 0xd2800000);// mov x16, #4

    bc_emit_u32(buffer, 0xd2800030);// svc #0x80
    bc_emit_u32(buffer, 0xd4001001);// mov x0, #0

    bc_emit_u32(buffer, 0x6c6c6548);// .ascii "Hello, world!"
    bc_emit_u32(buffer, 0x77202c6f);// .ascii "Hello, world!"
    bc_emit_u32(buffer, 0x646c726f);// .ascii "Hello, world!"
    bc_emit_u32(buffer, 0x00000021);// .ascii "Hello, world!"

     */
    for (u64 i = 0; i < buffer->size; i += 4) {
        u32 instruction = *(u32 *) (buffer->data + i);
        printf("%08x\n", instruction);
    }

    fwrite(buffer->data, 1, buffer->size, f);

    return true;
}