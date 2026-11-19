"""M11.9: final approval preparation must not grant Microsoft entitlement."""
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
import audit_redistribution_m119 as audit
import stage_release_m126 as stage
import prepare_m119_final_review as final


class M119FinalReviewTests(unittest.TestCase):
    def setUp(self):
        packages = {target: {'sha256': f'{i+1:064x}'}
                    for i, (target, _, _) in enumerate(stage.matrix.TARGETS)}
        self.expected = packages
        self.manifest = {'git_commit': '1' * 40,
                         'assets': [{'filename': f'ChromascopeIP-{stage.VERSION}-{target}{ext}',
                                     'sha256': packages[target]['sha256']}
                                    for target, ext, _ in stage.matrix.TARGETS]}

    def template(self):
        return audit.review_template('1'*40, self.expected)

    def test_only_actual_vc143_license_declaration_closes_last_decision(self):
        d = final.prepare_review(self.template(), self.manifest, 'SpilledMilkFactory', '2026-11-18', False)
        self.assertEqual(len(d['decisions']), 29)
        self.assertEqual(sum(v['decision'] == 'APPROVED' for v in d['decisions']), 28)
        self.assertEqual([v['topic'] for v in d['decisions'] if v['decision'] == 'BLOCKED'],
                         ['msvc-redistribution-rights'])

    def test_true_entitlement_confirmation_allows_complete_file(self):
        d = final.prepare_review(self.template(), self.manifest, 'SpilledMilkFactory', '2026-11-18', True)
        self.assertTrue(all(v['decision'] == 'APPROVED' for v in d['decisions']))
        self.assertTrue(all('Package SHA256 ' in v['evidence'] and len(v['evidence']) <= 300
                            for v in d['decisions']))

    def test_source_mismatch_fails_closed(self):
        t = self.template()
        t['source_commit'] = '2' * 40
        with self.assertRaises(audit.AuditError):
            final.prepare_review(t, self.manifest, 'SpilledMilkFactory', '2026-11-18', True)

    def test_digest_mismatch_fails_closed(self):
        t = self.template()
        t['packages']['Windows-x64'] = '3' * 64
        with self.assertRaises(audit.AuditError):
            final.prepare_review(t, self.manifest, 'SpilledMilkFactory', '2026-11-18', True)


if __name__ == '__main__':
    unittest.main()
