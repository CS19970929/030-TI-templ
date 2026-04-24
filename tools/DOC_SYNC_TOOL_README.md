# Codex 文档全局同步工具（自动覆盖所有对话工作目录）

脚本：`tools/doc_sync.ps1`

## 你要的模式
- 不再使用“当前对话目录”。
- 自动从 Codex 会话日志中提取所有对话的工作目录（`cwd`）。
- 对这些目录中的文档做“复制同步”（不是剪切）。

## 来源发现机制
- 扫描：`C:\Users\Administrator\.codex\sessions\**\*.jsonl`
- 每个会话文件第一行 `session_meta.payload.cwd` 作为工作目录来源。
- 自动去重，只同步存在的目录。

## 你只需要设置目标目录
```powershell
# 1) 设置集中管理目录（可随时修改）
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\doc_sync.ps1 -Action set-target -Path "D:\CodexDocs"

# 2) 查看识别到的对话工作目录
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\doc_sync.ps1 -Action list

# 3) 执行同步（复制，不剪切）
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\doc_sync.ps1 -Action sync
```

## 默认文档类型
- `*.md, *.markdown, *.txt, *.doc, *.docx, *.pdf`

## 跨对话生效
- 配置文件：`C:\Users\Administrator\.codex-doc-sync\config.json`
- 因此不依赖当前对话，所有后续对话都可复用同一目标目录。

## 可选项
- 若你的会话目录不在默认位置，可设置环境变量：
```powershell
setx CODEX_SESSIONS_DIR "C:\YourPath\.codex\sessions"
```
- 然后重新打开终端再执行 `list/sync`。
