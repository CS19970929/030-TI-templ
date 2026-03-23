import json
import os
import platform
import shutil
import subprocess
import sys


FALLBACKS = {
    "uv": [
        r"C:\Users\Administrator\AppData\Local\Microsoft\WinGet\Links\uv.exe",
    ],
    "cmake": [
        r"C:\Program Files\CMake\bin\cmake.exe",
    ],
    "ninja": [
        r"C:\Users\Administrator\AppData\Local\Microsoft\WinGet\Links\ninja.exe",
    ],
    "arm-none-eabi-gcc": [
        r"C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin\arm-none-eabi-gcc.exe",
    ],
}

TOOLS = ["python", "uv", "cmake", "ninja", "arm-none-eabi-gcc"]


def resolve_tool(tool: str):
    for candidate in FALLBACKS.get(tool, []):
        if os.path.exists(candidate):
            return candidate

    candidates = [tool]
    if tool == "python" and os.name != "nt":
        candidates.insert(0, "python3")

    for candidate in candidates:
        path = shutil.which(candidate)
        if path:
            return path
    return None


def check_arm_gcc_headers(gcc_path: str):
    completed = subprocess.run(
        [gcc_path, "-xc", "-E", "-"],
        input=b"#include <stdint.h>\n",
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        check=False,
    )
    stderr = completed.stderr.decode("utf-8", errors="replace").strip()
    if completed.returncode == 0:
        return True, ""
    if stderr:
        return False, stderr.splitlines()[-1]
    return False, "arm-none-eabi-gcc 无法预处理 <stdint.h>"


def main() -> int:
    report = {
        "platform": platform.platform(),
        "python": sys.version.split()[0],
        "python_ok": sys.version_info >= (3, 8),
        "tools": {},
        "checks": {},
    }
    missing = []

    for tool in TOOLS:
        path = resolve_tool(tool)
        report["tools"][tool] = path
        if path is None and tool != "uv":
            missing.append(tool)

    arm_gcc = report["tools"].get("arm-none-eabi-gcc")
    if arm_gcc:
        headers_ok, detail = check_arm_gcc_headers(arm_gcc)
        report["checks"]["arm-none-eabi-gcc-headers"] = {
            "ok": headers_ok,
            "detail": detail,
        }
        if not headers_ok:
            missing.append("arm-none-eabi-gcc headers")

    print(json.dumps(report, indent=2, ensure_ascii=False))

    if not report["python_ok"]:
        print("\n默认 Python 版本过低，建议至少使用 Python 3.8。")
        return 1

    if missing:
        print("\n缺少工具: " + ", ".join(missing))
        return 1

    print("\n工具链检查通过。")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
