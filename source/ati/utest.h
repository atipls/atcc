#pragma once

#include "basic.h"
#include "string.h"

typedef int(*UTestFn)();

typedef struct UTest {
    string name;
    UTestFn fn;
} UTest;

#define UTEST_DEAD 2
#define UTEST_FAIL 1
#define UTEST_PASS 0

// TODO: Assert messages with filename and location?
#define UASSERT(expr) if (!expr) return UTEST_FAIL;

void utest_register(string name, UTest *tests, u32 count);

void utest_run();