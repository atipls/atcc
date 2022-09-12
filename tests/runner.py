#!/usr/bin/env python3

import os
import sys
import subprocess

ARGS = sys.argv[1:]
DEBUG = len(ARGS) > 0 and ARGS[0].lower() == "debug"
COMPILER_PATH = "build/atcc" if not DEBUG else "debug/atcc"


def run_test_stage(path, stage, command):
    result = subprocess.run(command, capture_output=True)
    if result.returncode != 0:
        print(f"FAIL[{stage}]: {path}")
        print(result.stdout.decode("utf-8"))
        print(result.stderr.decode("utf-8"))
        return False

    return True


def run_a_test(path):
    if not run_test_stage(path, "ATCC", [COMPILER_PATH, path]):
        return False

    if not run_test_stage(path, "COMP", ["gcc", "-o", "testexec", "generated.c"]):
        return False

    if not run_test_stage(path, "EXEC", ["./testexec"]):
        return False

    print(f"PASS: {path}")
    return True


passed = 0
failed = 0

for file in sorted(os.listdir("tests/cases")):
    if run_a_test(os.path.join("tests/cases", file)):
        passed += 1
    else:
        failed += 1

print(f"Passed: {passed}, Failed: {failed}, Total: {passed + failed}")
