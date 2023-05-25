#include "ati/utest.h"
#include "ati/utils.h"
#include "emit/bytecode.h"

static int bc_test_initialization(void) {
    BCContext context = bc_context_initialize();

    return UTEST_PASS;
}

void bc_register_utest(void) {
	UTest tests[] = {
		{ str("initialization"), bc_test_initialization },
	};

	utest_register(str("bytecode"), tests, array_length(tests));
}
