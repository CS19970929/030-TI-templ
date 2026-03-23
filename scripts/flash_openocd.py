import argparse
import ctypes
import os
import subprocess
from pathlib import Path


def to_windows_short_path(path: Path) -> str:
    if os.name != "nt":
        return path.as_posix()

    kernel32 = ctypes.windll.kernel32
    buffer_size = 4096
    buffer = ctypes.create_unicode_buffer(buffer_size)
    result = kernel32.GetShortPathNameW(str(path), buffer, buffer_size)
    if result == 0 or result > buffer_size:
        return path.as_posix()
    return buffer.value.replace("\\", "/")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="使用 OpenOCD 烧录 STM32 固件。")
    parser.add_argument("--openocd", required=True, help="openocd.exe 路径")
    parser.add_argument("--scripts-dir", required=True, help="OpenOCD scripts 目录")
    parser.add_argument("--artifact", required=True, help="待烧录的 bin 文件路径")
    parser.add_argument("--address", default="0x08001C00", help="烧录地址")
    parser.add_argument("--interface", default="interface/stlink.cfg", help="接口配置")
    parser.add_argument("--target", default="target/stm32f0x.cfg", help="目标配置")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    openocd = Path(args.openocd)
    scripts_dir = Path(args.scripts_dir)
    artifact = Path(args.artifact).resolve()

    if not openocd.exists():
        raise SystemExit(f"OpenOCD 不存在: {openocd}")
    if not scripts_dir.exists():
        raise SystemExit(f"OpenOCD scripts 目录不存在: {scripts_dir}")
    if not artifact.exists():
        raise SystemExit(f"固件文件不存在: {artifact}")

    command = [
        str(openocd),
        "-s",
        str(scripts_dir),
        "-f",
        args.interface,
        "-f",
        args.target,
        "-c",
        f"program {to_windows_short_path(artifact)} {args.address} verify reset exit",
    ]
    print("flash command:")
    print(" ".join(command))
    return subprocess.run(command, check=False).returncode


if __name__ == "__main__":
    raise SystemExit(main())
