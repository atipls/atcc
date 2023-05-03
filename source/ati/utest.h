#pragma once

#include "basic.h"
#include "string.h"

typedef int (*UTestFn)(void);

typedef struct UTest {
    string name;
    UTestFn fn;
} UTest;

#define UTEST_DEAD 2
#define UTEST_FAIL 1
#define UTEST_PASS 0

#define UTEST_FAIL_MESSAGE(expr)                                                   \
    do {                                                                           \
        fprintf(stderr, "%s:%d: Assertion hit: " #expr " \n", __FILE__, __LINE__); \
        return UTEST_FAIL;                                                         \
    } while (0)

#define UASSERT(expr)                 \
    do {                              \
        if (!(expr))                  \
            UTEST_FAIL_MESSAGE(expr); \
    } while (0)

void utest_register(string name, UTest *tests, u32 count);

int utest_run(void);
