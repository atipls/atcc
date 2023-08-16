import os
import sys

from .utils import execute, ExecutionFailed

STATUS_SUCCESS = "Success"
STATUS_BROKEN = "Marked as broken"
STATUS_ATCC = "Running ATCC"
STATUS_COMPILER = "Running the C Compiler"
STATUS_EXECUTION = "Executing the compiled program"

DISABLED_WARNINGS = [
    "-Wno-pointer-sign",
    "-Wno-incompatible-library-redeclaration",
    "-Wno-incompatible-pointer-types",
    # "-Wno-builtin-declaration-mismatch"
]


class CompilationTestFailed(Exception):
    def __init__(self, test, where, stdout, stderr):
        self.test = test
        self.where = where
        self.stdout = stdout
        self.stderr = stderr

    def __str__(self):
        return f"Compilation test failed at {self.where}:\n{self.stdout}\n{self.stderr}"


class Test:
    def __init__(self, compiler, path, optimize=False, debug=False, broken=False, arguments=None):
        if arguments is None:
            arguments = []
        self.compiler = compiler
        self.path = path
        self.optimize = optimize
        self.debug = debug
        self.broken = broken

        self.stdout = ""
        self.stderr = ""

        self.atcc_name = f"test_{os.path.basename(path)}"
        self.executable_name = f"{self.atcc_name}_runner"

        self.arguments = arguments

    def try_execute_with_status(self, where, command):
        try:
            execute(command)
        except ExecutionFailed as e:
            raise CompilationTestFailed(self, where, e.stdout, e.stderr)

    def execute(self):
        try:
            self.try_execute_with_status(STATUS_ATCC, [self.compiler, "-verbose-all", "-o", self.atcc_name, self.path])
            self.try_execute_with_status(STATUS_COMPILER, self.generate_compiler_command())
            self.try_execute_with_status(STATUS_EXECUTION, ["./" + self.executable_name, *self.arguments])
        except CompilationTestFailed as e:
            self.stdout = e.stdout
            self.stderr = e.stderr
            return e.where
        finally:
            execute(["rm", self.atcc_name + ".c", self.executable_name], ignore_result=True)
            if self.debug and sys.platform == "darwin":
                execute(["rm", "-rf", self.executable_name + ".dSYM"], ignore_result=True)

        return STATUS_SUCCESS

    def generate_compiler_command(self):
        command = ["cc"]

        if self.optimize:
            command.append("-O3")
        if self.debug:
            command.append("-g")

        if "ADDRESS_SANITIZER" in os.environ:
            command.append("-fsanitize=address")

        command.extend(DISABLED_WARNINGS)

        command.extend(["-o", self.executable_name, f"{self.atcc_name}.c"])

        return command

    def __str__(self):
        return f"Test({self.path}, optimize={self.optimize}, debug={self.debug}, broken={self.broken}, arguments={self.arguments})"
