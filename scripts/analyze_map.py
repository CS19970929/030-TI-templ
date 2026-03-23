import argparse
import json
import re
from pathlib import Path


LINE_RE = re.compile(r"^\s+\.(?P<section>\S+)\s+0x[0-9a-fA-F]+\s+0x(?P<size>[0-9a-fA-F]+)\s+(?P<object>.+)$")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="分析 ARM map 文件中的对象大小热点。")
    parser.add_argument("--map", required=True, help="map 文件路径")
    parser.add_argument("--top", type=int, default=20, help="输出前 N 项")
    parser.add_argument("--json", help="可选 JSON 输出路径")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    map_path = Path(args.map)
    if not map_path.exists():
        raise SystemExit(f"map 文件不存在: {map_path}")

    entries = []
    for line in map_path.read_text(encoding="utf-8", errors="ignore").splitlines():
        match = LINE_RE.match(line)
        if not match:
            continue
        entries.append(
            {
                "section": match.group("section"),
                "size": int(match.group("size"), 16),
                "object": match.group("object").strip(),
            }
        )

    entries.sort(key=lambda item: item["size"], reverse=True)
    top_entries = entries[: args.top]

    for item in top_entries:
        print(f"{item['size']:>8}  {item['section']:<16}  {item['object']}")

    if args.json:
        output_path = Path(args.json)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(json.dumps(top_entries, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
