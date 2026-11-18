"""M10.5 (#71) source-visible security boundary audit regression sentinels.

No model loading, camera access, decoder execution, or filesystem mutation.
These checks are not a penetration test or third-party vulnerability scan.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
DOC = ROOT / "docs/history/milestones/M10_5_THREAT_BOUNDARY_AUDIT.md"


def source(relative):
    return (ROOT / relative).read_text(encoding="utf-8")


class ThreatBoundaryAuditTests(unittest.TestCase):
    def test_review_scope_and_traceable_register(self):
        audit = DOC.read_text(encoding="utf-8")
        for boundary in ("B-FS-01", "B-FS-02", "B-IMG-01", "B-VID-01",
                         "B-MOD-01", "B-CAM-01", "B-RES-01", "B-DEP-01"):
            self.assertIn(boundary, audit)
        for risk in range(1, 9):
            self.assertRegex(audit, rf"\| R-{risk:02d} \| \*\*?P[123]\*\*? \|")
        for phrase in ("untrusted content", "No confirmed critical exploit",
                       "not a penetration test", "third-party", "OPEN",
                       "verification", "follow-up"):
            self.assertIn(phrase.lower(), audit.lower())

    def test_still_image_predecode_budget_and_residual_parser_risk(self):
        code = source("src/core/ImageIO.cpp")
        self.assertLess(code.index("inspect_image_header(encoded)"), code.index("cv::imdecode"))
        self.assertLess(code.index("resources::validate_image(header.width"), code.index("cv::imdecode"))
        self.assertIn("max_encoded_bytes", code)
        self.assertIn("B-IMG-01", DOC.read_text(encoding="utf-8"))

    def test_video_decode_postcheck_is_not_claimed_predecode(self):
        code = source("src/core/VideoSource.cpp")
        self.assertIn("is_regular_file(path", code)
        self.assertIn("validate_reported_dimensions", code)
        self.assertLess(code.index("capture.read(decoded)"), code.index("resources::validate_image(decoded.cols"))
        self.assertIn("before", DOC.read_text(encoding="utf-8").split("B-VID-01")[1].lower())

    def test_model_loader_budgets_do_not_substitute_for_parser_isolation(self):
        code = source("src/core/CpuDnnAdapter.cpp")
        self.assertIn("kMaxOnnxBytes", code)
        self.assertIn("readNetFromONNX", code)
        self.assertIn("DNN_TARGET_CPU", code)
        self.assertIn("kMaxOutputFloats", code)
        digest = source("src/core/DnnModelSha256.cpp")
        self.assertIn("dnn_file_sha256", digest)
        self.assertIn("B-MOD-01", DOC.read_text(encoding="utf-8"))

    def test_camera_opt_in_backend_and_blocking_risk_are_documented(self):
        source_code = source("src/core/CameraSource.cpp")
        discovery = source("src/core/CameraDiscovery.cpp")
        self.assertIn("device_index > 255", source_code)
        self.assertIn("preferred_camera_backend()", source_code)
        self.assertIn("allow_device_open", discovery)
        self.assertIn("no timeout is promised", discovery)
        self.assertIn("R-03", DOC.read_text(encoding="utf-8"))

    def test_output_paths_have_different_policies_not_universal_no_clobber(self):
        paths = source("src/cli/CliOutputPaths.hpp")
        stream = source("src/cli/CameraCliDetection.cpp")
        exported = source("src/core/AnnotatedVideoExport.cpp")
        cli = source("src/cli/main.cpp")
        self.assertIn("weakly_canonical", paths)
        self.assertIn("wbx", stream)
        self.assertIn("create_hard_link", exported)
        self.assertIn("std::ios::trunc", cli)
        self.assertIn("not create-only", DOC.read_text(encoding="utf-8"))

    def test_dependencies_and_sanitizer_scope_are_candid(self):
        policy = source("cmake/ChromascopeQuality.cmake")
        cmake = source("CMakeLists.txt")
        dep = source(".github/dependabot.yml")
        audit = DOC.read_text(encoding="utf-8")
        self.assertIn("CHROMASCOPE_ENABLE_SANITIZERS", policy)
        self.assertIn("v1.93.0", cmake)
        self.assertIn("github-actions", dep)
        self.assertIn("not freedom from CVEs", audit)
        self.assertIn("first-party", audit)

    def test_no_claim_of_full_mitigation_or_platform_certification(self):
        audit = DOC.read_text(encoding="utf-8")
        self.assertIn("Windows Server", audit)
        self.assertIn("not a whole-process memory/CPU upper bound", audit)
        self.assertIn("Do not close a P1", audit)
        self.assertIn("No confirmed critical exploit", audit)
        self.assertIn("cannot conclude that none exists", audit)


if __name__ == "__main__":
    unittest.main()
