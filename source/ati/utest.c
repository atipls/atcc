#include "utest.h"
#include "utils.h"

#include <setjmp.h>
#include <string.h>

static jmp_buf utest_recovery;

static UTest *utests = null;

static void utest_signal_handler(i32 sig) {
    longjmp(utest_recovery, UTEST_DEAD);
}

static int utest_sort_by_name(const void *s1, const void *s2) {
    UTest *a = (UTest *) s1;
    UTest *b = (UTest *) s2;

    if (a->name.length < b->name.length)
        return -1;
    if (a->name.length > b->name.length)
        return 1;
    return memcmp(a->name.data, b->name.data, a->name.length);
}

void utest_register(string name, UTest *tests, u32 count) {
    for (u32 i = 0; i < count; i++) {
        UTest *test = vector_add(utests, 1);

        test->fn = tests[i].fn;
        test->name = make_string(name.length + tests[i].name.length + 2);

        snprintf((char *) test->name.data, test->name.length, "%.*s:%.*s", strp(name), strp(tests[i].name));
    }

    qsort(utests, vector_len(utests), sizeof(UTest), utest_sort_by_name);
}

void utest_run() {
    sig_t old_sigsegv, old_sigabrt;

    old_sigsegv = signal(SIGSEGV, utest_signal_handler);
    old_sigabrt = signal(SIGABRT, utest_signal_handler);

    i32 test_pass = 0, test_fail = 0;

    vector_foreach(UTest, test, utests) {
        printf("[UTEST] Running \033[1m%.*s\033[0m... ", strp(test->name));

        if (!setjmp(utest_recovery)) {
            if (test->fn() == UTEST_PASS) {
                printf("\033[32m PASS\033[0m\n");
                test_pass++;
            } else {
                printf("\033[31m FAIL\033[0m\n");
                test_fail++;
            }
        } else {
            printf("\033[31m DEAD\033[0m\n");
            test_fail++;
        }
    }

    printf("[UTEST] \033[32m%d\033[0m passed, \033[31m%d\033[0m failed.\n", test_pass, test_fail);

    signal(SIGSEGV, old_sigsegv);
    signal(SIGABRT, old_sigabrt);
}