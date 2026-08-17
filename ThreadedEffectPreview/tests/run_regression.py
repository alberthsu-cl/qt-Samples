"""Build and run the headless Qt worker-thread regression test.

This script deliberately treats the C++ test program as a black box: it builds
the requested CMake target, runs CTest, writes the output to a timestamped log,
and returns the same pass/fail exit code to a CI system.
"""

from __future__ import annotations

import argparse
import datetime as dt
import os
from pathlib import Path
import subprocess
import sys


PROJECT_ROOT = Path(__file__).resolve().parents[1]
BUILD_DIRECTORY = PROJECT_ROOT / "out" / "build" / "vs2022-x64"
RESULTS_DIRECTORY = PROJECT_ROOT / "test-results"


def find_cmake() -> Path:
    """Use PATH first, then Visual Studio's bundled CMake on Windows."""
    configured_path = os.environ.get("CMAKE_COMMAND")
    if configured_path:
        return Path(configured_path)

    visual_studio_editions = ("Community", "Professional", "Enterprise")
    for edition in visual_studio_editions:
        candidate = Path("C:/Program Files/Microsoft Visual Studio/2022") / edition \
            / "Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
        if candidate.is_file():
            return candidate

    # Let Windows or another platform resolve this from PATH.
    return Path("cmake")


def run(command: list[str], environment: dict[str, str]) -> subprocess.CompletedProcess[str]:
    print("+", " ".join(command))
    return subprocess.run(
        command,
        cwd=PROJECT_ROOT,
        env=environment,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--configuration",
        choices=("Debug", "Release"),
        default="Debug",
        help="CMake configuration to build and test (default: Debug).",
    )
    arguments = parser.parse_args()

    build_preset = f"vs2022-{arguments.configuration.lower()}"
    cmake = find_cmake()
    ctest = cmake.with_name("ctest.exe") if cmake.name.lower() == "cmake.exe" else Path("ctest")
    environment = os.environ.copy()
    # The current C++ test is guiless, but this keeps the runner suitable when
    # a later test creates a QWidget for a headless integration exercise.
    environment.setdefault("QT_QPA_PLATFORM", "offscreen")

    configure_result = run([str(cmake), "--preset", "vs2022-x64"], environment)
    build_result = run(
        [str(cmake), "--build", "--preset", build_preset,
         "--target", "ThreadedEffectPreviewTests"],
        environment,
    ) if configure_result.returncode == 0 else configure_result

    RESULTS_DIRECTORY.mkdir(exist_ok=True)
    timestamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    log_path = RESULTS_DIRECTORY / f"threaded-effect-preview-{timestamp}.log"

    if build_result.returncode == 0:
        test_result = run(
            [str(ctest), "--test-dir", str(BUILD_DIRECTORY),
             "-C", arguments.configuration, "--output-on-failure"],
            environment,
        )
        output = test_result.stdout
        exit_code = test_result.returncode
    else:
        output = build_result.stdout
        exit_code = build_result.returncode

    log_path.write_text(output, encoding="utf-8")
    print(output, end="")
    print(f"Regression log: {log_path}")
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
