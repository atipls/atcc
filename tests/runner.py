#!/usr/bin/env python3

import sys, os
from test import TestSetup
from test.utils import execute

compiler = TestSetup.get_compiler(sys.argv[1:])
TestSuite, Test = TestSetup.setup_suite(compiler)


def Case(path, *args, **kwargs):
    return Test(f"tests/{path}", *args, **kwargs)


tests = [
    # Case("cases/01-main.aa", arguments=["test", "hello"]),
    Case("cases/02-control.aa"),
    Case("cases/03-string.aa"),
    # Case("cases/04-array.aa"),
    Case("cases/05-enum.aa"),
]

suite = TestSuite(tests)

suite.run()
suite.print_results()

utest = execute([compiler, "--utest"], ignore_result=True)

sys.exit(1 if not utest or suite.tests_did_fail() else 0)
