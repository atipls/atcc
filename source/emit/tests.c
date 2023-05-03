#include "ati/utest.h"
#include "ati/utils.h"
#include "bytecode.h"

static int bc_test_initialization(void) {
    BCContext context = bc_context_initialize();

    return UTEST_PASS;
}

static UTest utests_bc[] = {
    { str("initialization"), bc_test_initialization },
};

void bc_register_utest(void) {
    utest_register(str("bytecode"), utests_bc, array_length(utests_bc));
    
}
