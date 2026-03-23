import argparse
import shutil
import subprocess
import tempfile
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="使用 J-Link Commander 烧录 STM32 固件。")
    parser.add_argument("--jlink", required=True, help="JLink.exe 路径")
    parser.add_argument("--device", required=True, help="目标芯片型号，例如 STM32F030C8")
    parser.add_argument("--artifact", required=True, help="待烧录的 bin 文件路径")
    parser.add_argument("--address", default="0x08001C00", help="烧录地址")
    parser.add_argument("--interface", default="SWD", help="调试接口")
    parser.add_argument("--speed", default="4000", help="接口速度，单位 kHz")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    jlink = Path(args.jlink)
    artifact = Path(args.artifact).resolve()

    if not jlink.exists() and shutil.which(str(jlink)) is None:
        raise SystemExit(f"J-Link 不存在: {jlink}")
    if not artifact.exists():
        raise SystemExit(f"固件文件不存在: {artifact}")

    script_content = "\n".join(
        [
            f"device {args.device}",
            f"si {args.interface}",
            f"speed {args.speed}",
            "r",
            f"loadbin {artifact.as_posix()} {args.address}",
            "r",
            "g",
            "qc",
            "",
        ]
    )

    with tempfile.NamedTemporaryFile("w", suffix=".jlink", delete=False, encoding="utf-8") as handle:
        handle.write(script_content)
        script_path = Path(handle.name)

    command = [str(jlink), "-CommandFile", str(script_path)]
    print("flash command:")
    print(" ".join(command))
    return subprocess.run(command, check=False).returncode


if __name__ == "__main__":
    raise SystemExit(main())
