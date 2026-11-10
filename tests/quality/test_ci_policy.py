"""Regression tests for the GitHub Actions security-policy checker."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[2] / "scripts/check_ci_policy.py"
spec = importlib.util.spec_from_file_location("ci_policy", SCRIPT)
policy = importlib.util.module_from_spec(spec)
spec.loader.exec_module(policy)

VALID = """name: CI
on:
  pull_request:
permissions:
  contents: read
jobs:
  test:
    steps:
      - uses: actions/checkout@1111111111111111111111111111111111111111
        with:
          persist-credentials: false
"""


class WorkflowPolicyTests(unittest.TestCase):
    def issues(self, value):
        with tempfile.TemporaryDirectory() as temp:
            file = Path(temp) / 'workflow.yml'
            file.write_text(value, encoding='utf-8')
            return policy.check_workflow(file)

    def test_valid_pin(self):
        self.assertEqual([], self.issues(VALID))

    def test_reject_floating_tag(self):
        self.assertTrue(self.issues(VALID.replace('@' + '1'*40, '@v4')))

    def test_reject_privileged_trigger(self):
        self.assertTrue(self.issues(VALID.replace('pull_request:', 'pull_request_target:')))

    def test_reject_write_token(self):
        self.assertTrue(self.issues(VALID.replace('contents: read', 'contents: write')))

    def test_reject_checkout_credentials(self):
        self.assertTrue(self.issues(VALID.replace('persist-credentials: false', 'persist-credentials: true')))


if __name__ == '__main__':
    unittest.main()
