#!/usr/bin/env python3
"""Run a targeted clang-tidy gate on registered first-party C++ units only."""
import argparse
import json
from pathlib import Path
import subprocess
import sys


def first_party_sources(database, root):
    root = root.resolve()
    prefixes = ("src/core/", "src/cli/", "tests/unit/",
                "tests/integration/", "tests/regression/")
    result = set()
    for item in database:
        source = Path(item["file"])
        if not source.is_absolute():
            source = Path(item["directory"]) / source
        source = source.resolve()
        try:
            relative = source.relative_to(root).as_posix()
        except ValueError:
            continue
        if relative.endswith(".cpp") and relative.startswith(prefixes):
            result.add(source)
    return sorted(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--tool", default="clang-tidy-18")
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    build_dir = args.build_dir.resolve()
    database_path = build_dir / "compile_commands.json"
    if not database_path.is_file():
        parser.error(f"Missing compilation database: {database_path}")
    database = json.loads(database_path.read_text(encoding="utf-8"))
    selected = first_party_sources(database, root)
    required = {
        "src/core/Detection.cpp", "src/core/ImageIO.cpp", "src/cli/main.cpp",
        "src/core/DetectionResult.cpp", "src/core/DetectionResultBridge.cpp",
        "src/core/Processor.cpp", "src/core/SkittlesProcessor.cpp",
        "src/core/ProcessingQueue.cpp",
        "src/core/Annotation.cpp", "src/core/AnnotationBridge.cpp",
        "src/core/AnnotationRendererCV.cpp",
        "src/core/LegacyAnnotationAdapter.cpp",
        "tests/unit/test_annotation.cpp",
        "tests/integration/test_annotation_renderer.cpp",
        "tests/unit/test_processing_queue.cpp", "tests/integration/test_processing_async.cpp",
        "tests/unit/test_processor_registry.cpp", "tests/integration/test_processor_skittles.cpp",
        "tests/unit/test_detection_result.cpp", "tests/integration/test_detection_result.cpp",
        "tests/unit/test_color.cpp", "tests/unit/test_validation.cpp",
        "tests/integration/test_synthetic.cpp", "tests/regression/test_skittles.cpp",
    }
    selected_names = {s.relative_to(root).as_posix() for s in selected}
    if not required.issubset(selected_names):
        parser.error(f"Missing first-party units from compilation database: {sorted(required - selected_names)}")
    print(f"Analyzing {len(selected)} first-party translation units", flush=True)
    for source in selected:
        print(f"clang-tidy: {source.relative_to(root)}", flush=True)
        subprocess.run([args.tool, "--quiet", f"-p={build_dir}",
                        f"--config-file={root / '.clang-tidy'}", str(source)],
                       check=True, cwd=root)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (subprocess.CalledProcessError, FileNotFoundError) as exc:
        print(f"Static-analysis gate failed: {exc}", file=sys.stderr)
        sys.exit(1)
