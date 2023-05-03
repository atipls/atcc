import sys, subprocess


class ExecutionFailed(Exception):
    def __init__(self, command, stdout, stderr):
        self.command = command
        self.stdout = stdout
        self.stderr = stderr

    def __str__(self):
        return f"Execution failed: {self.command}\n{self.stdout}\n{self.stderr}"


def execute(command, ignore_result=False, with_stdout=False):
    if with_stdout:
        result = subprocess.run(command, stderr=sys.stderr, stdout=sys.stdout)
    else:
        result = subprocess.run(command, capture_output=not ignore_result)

    success = result.returncode == 0

    if not success and not ignore_result:
        raise ExecutionFailed(command, result.stdout, result.stderr)

    return success
