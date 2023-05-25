import functools

from .utils import execute
from .suite import TestSuite
from .test import Test

COMPILER_PATHS = {
    "xcode:debug": "build/xcode/Debug/atcc",
    "xcode:release": "build/xcode/Release/atcc",
    "clion:debug": "build/debug/atcc",
    "clion:release": "build/release/atcc",
    "ci": "cibuild/atcc",
}


class TestSetup:
    @staticmethod
    def setup_ci_compiler():
        execute(["mkdir", "-p", "cibuild"], ignore_result=True, with_stdout=True)
        execute(["cmake", "-S", ".", "-B", "cibuild", "-DCMAKE_BUILD_TYPE=Debug"], with_stdout=True)
        execute(["cmake", "--build", "cibuild"], with_stdout=True)

    @staticmethod
    def get_compiler(args):
        if len(args) < 1:
            return COMPILER_PATHS["clion:release"]

        build_type = args[0].lower()
        if build_type == "ci":
            TestSetup.setup_ci_compiler()
            return COMPILER_PATHS["ci"]

        return COMPILER_PATHS.get(build_type)

    @staticmethod
    def setup_suite(compiler):
        partial_suite = functools.partial(TestSuite, compiler)
        partial_test = functools.partial(Test, compiler)

        return partial_suite, partial_test
