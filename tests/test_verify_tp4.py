"""Ensure compiler failures cannot pass as expected source errors."""
import subprocess
import unittest

from verify_tp4 import verify_compilation


class CompilationVerificationTests(unittest.TestCase):
    def result(self, stderr='', returncode=1, stdout=''):
        return subprocess.CompletedProcess(['compiler'], returncode, stdout, stderr)

    def test_expected_diagnostic(self):
        verify_compilation(self.result('Line 7: Semantic error: Incompatible return type: SINGLEF\n'),
                           r'return type: (INTEGER|SINGLEF)')

    def test_wrong_diagnostic(self):
        with self.assertRaises(AssertionError):
            verify_compilation(self.result('Line 7: Syntax error: unexpected token\n'), 'return type')

    def test_sanitizer_failure(self):
        for diagnostic in ('ERROR: AddressSanitizer: heap-buffer-overflow',
                           'AddressSanitizer:DEADLYSIGNAL',
                           'ERROR: LeakSanitizer: detected memory leaks',
                           'LeakSanitizer has encountered a fatal error.',
                           'SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior',
                           'src/types.c:327:12: runtime error: index out of bounds'):
            with self.subTest(diagnostic=diagnostic), self.assertRaises(AssertionError):
                verify_compilation(self.result('Line 7: Semantic error: return type\n' + diagnostic),
                                   'return type')

    def test_crash(self):
        with self.assertRaises(AssertionError):
            verify_compilation(self.result('return type', returncode=-11), 'return type')

    def test_partial_output(self):
        with self.assertRaises(AssertionError):
            verify_compilation(self.result('return type', stdout='--- RPN ---'), 'return type')

    def test_success(self):
        verify_compilation(self.result(returncode=0, stdout='--- RPN ---'))

    def test_identifier_named_like_sanitizer(self):
        verify_compilation(self.result("Line 1: Lexical error: Invalid identifier 'AddressSanitizer'\n"),
                           'Invalid identifier')


if __name__ == '__main__':
    unittest.main()
