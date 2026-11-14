"""M7.6 source-facing, offline privacy regression gates (not a network audit)."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


class PrivacyContractTests(unittest.TestCase):
    def test_gui_layout_and_log_files_disabled_before_backends(self):
        code = (ROOT / "src/gui/main.cpp").read_text(encoding="utf-8")
        init = code.index("ImGui::CreateContext();")
        ini = code.index("ImGui::GetIO().IniFilename = nullptr;")
        log = code.index("ImGui::GetIO().LogFilename = nullptr;")
        backend = code.index("ImGui_ImplGlfw_InitForOpenGL(")
        self.assertLess(init, ini)
        self.assertLess(ini, log)
        self.assertLess(log, backend)
        self.assertIn('"Clear loaded media, model and results"', code)
        self.assertIn('"Local privacy and session controls"', code)

    def test_session_reset_invalidation_and_export_retention(self):
        code = (ROOT / "src/gui/main.cpp").read_text(encoding="utf-8")
        reset = code.split("void clear_private_session() {", 1)[1].split("void save_person() {", 1)[0]
        for action in ("processing->cancel()", "detector_session.unload()",
                       "video_export_worker.request_stop()", "video_detection.cancel()",
                       "video_player.close()", "input_frame.reset()",
                       "original_texture.reset()", "video_texture.reset()",
                       "detector_model_path.fill('\\0')", "video_export_audio_ack = false"):
            self.assertIn(action, reset)
        self.assertNotIn("remove_all(", reset)
        self.assertNotIn("std::filesystem::remove(", reset)

    def test_public_policy_has_scope_and_caveats(self):
        policy = (ROOT / "docs/M7_6_LOCAL_PRIVACY.md").read_text(encoding="utf-8")
        for claim in ("third-party", "COCO class", "no face detection",
                      "not a secure wipe", "not anonymization", "not securely erased",
                      "audio", "stdout", "imgui.ini", "staging", "OS"):
            self.assertIn(claim.casefold(), policy.casefold())
        self.assertIn("--privacy-info", (ROOT / "src/cli/main.cpp").read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
