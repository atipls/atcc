from .test import STATUS_SUCCESS


class TestSuite:
    def __init__(self, compiler, tests):
        self.compiler = compiler
        self.tests = tests

        self.passed = []
        self.failed = []
        self.skipped = []

    def run(self):
        for test in self.tests:
            print(f"Running test {test.path}... ", end="")
            if test.broken:
                self.skipped.append(test)
                continue

            result = test.execute()
            print(result)

            if result == STATUS_SUCCESS:
                self.passed.append(test)
            else:
                self.failed.append((test, result))

    def print_results(self):
        print(f"Passed: {len(self.passed)}, Failed: {len(self.failed)}, Skipped: {len(self.skipped)}")

        if len(self.failed) > 0:
            print("Failed tests:")
            for (test, where) in self.failed:
                print(f"  {test.path} ({where})")
                print("  --- stdout ---")
                print(test.stdout.decode("utf-8", errors="ignore"))
                print("  --- stderr ---")
                print(test.stderr.decode("utf-8", errors="ignore"))

        if len(self.skipped) > 0:
            print("Skipped tests:")
            for test in self.skipped:
                print(f"  {test.path}")

    def tests_did_fail(self):
        return len(self.failed) > 0
