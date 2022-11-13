#!/usr/bin/env python3

import os
import sys
import subprocess

ARGS = sys.argv[1:]
DEBUG = len(ARGS) > 0 and ARGS[0].lower() == "debug"
COMPILER_PATH = "build/atcc" if not DEBUG else "debug/atcc"

DISABLED_WARNINGS = [
    "-Wno-pointer-sign",
    "-Wno-incompatible-library-redeclaration",
    "-Wno-incompatible-pointer-types",
    # "-Wno-builtin-declaration-mismatch"
]


def run_test_stage(path, stage, command):
    result = subprocess.run(command, capture_output=True)
    stderr_output = result.stderr.decode("utf-8")
    if result.returncode != 0:
        print(f"FAIL[{stage}]: {path}")
        print(result.stdout.decode("utf-8"))
        print(stderr_output)
        return False

    if stage == "COMP" and len(stderr_output) > 0:
        print(stderr_output)

    return True


def run_a_test(path):
    if not run_test_stage(path, "ATCC", [COMPILER_PATH, "tests/preload.aa", path]):
        return False

    if not run_test_stage(path, "COMP", ["gcc", "-O3", "-g", *DISABLED_WARNINGS, "-o", "testexec", "generated.c"]):
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
