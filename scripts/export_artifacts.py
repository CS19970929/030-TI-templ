import argparse
import json
import shutil
from pathlib import Path


def normalize_path(raw_path: str) -> Path:
    path = Path(raw_path)
    if path.is_absolute():
        return path
    return Path.cwd() / path


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="导出构建计划或清理 artifacts 目录。")
    parser.add_argument("--source-root", required=True, help="仓库根目录")
    parser.add_argument("--output", required=True, help="输出文件路径")
    parser.add_argument("--clean", action="store_true", help="清理 artifacts 目录")
    return parser


def main() -> int:
    args = build_parser().parse_args()
    source_root = normalize_path(args.source_root)
    artifacts_dir = source_root / "artifacts"

    if args.clean:
        if artifacts_dir.exists():
            shutil.rmtree(artifacts_dir)
        print(f"已清理目录: {artifacts_dir}")
        return 0

    output_path = normalize_path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    plan = {
        "source_root": str(source_root),
        "artifacts_dir": str(artifacts_dir),
        "status": "ready",
        "next_step": "执行 task build 生成 GCC 构建产物",
    }
    output_path.write_text(json.dumps(plan, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"已生成构建计划: {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
