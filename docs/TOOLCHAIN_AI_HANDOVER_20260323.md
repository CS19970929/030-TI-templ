# 030 + TI 工具链接管说明

## 目标

本仓库已补齐一套与 Keil 并行存在的 GCC/CMake/Task/Python 工具链，目的是让后续 AI 或脚本可以在不进入 Keil GUI 的前提下完成以下动作：

- 检查本机是否具备构建工具
- 以统一命令入口构建 STM32F030 固件
- 清理和分析自动化构建产物
- 保持原有 Keil 工程继续可打开、可编译、可调试

## 当前工具链入口

- `task doctor`
  - 检查 `python`、`uv`、`cmake`、`ninja`、`arm-none-eabi-gcc`
- `task init`
  - 初始化 Python 工具环境
- `task build`
  - 使用 `firmware-release` Preset 执行 GCC 构建
- `task build-debug`
  - 使用 `firmware-debug` Preset 执行 GCC Debug 构建
- `task clean`
  - 清理 `artifacts/`
- `task test`
  - 对脚本入口做基础自检
- `task map`
  - 分析 map 文件热点

## 关键文件

- `Taskfile.yml`
  - 统一命令入口
- `CMakePresets.json`
  - 预设 GCC Debug/Release 构建目录
- `toolchains/arm-none-eabi-gcc.cmake`
  - Arm GNU Toolchain 自动发现逻辑
- `firmware/CMakeLists.txt`
  - 固件源文件、头文件、宏定义、链接输出规则
- `firmware/linker/stm32f030c8_bq769x0_app.ld`
  - 与 Keil 保持一致的 Flash/RAM 布局
- `firmware/startup/startup_stm32f0xx_gcc.S`
  - GCC 启动文件
- `scripts/check_toolchain.py`
  - 本机工具链巡检
- `scripts/export_artifacts.py`
  - 清理或导出构建计划
- `scripts/analyze_map.py`
  - map 热点分析

## 内存布局约束

当前 GCC 链接脚本按 Keil 工程中的布局迁移，关键地址如下：

- Flash 起始地址：`0x08001C00`
- Flash 长度：`0xE400`
- RAM Vector 区：`0x20000000`，长度 `0xC0`
- 主 RAM 区：`0x200000C0`，长度 `0x1F40`
- 默认 Heap：`0x200`
- 默认 Stack：`0xC00`

不要在没有评审的前提下直接修改这些值，因为它们关系到 IAP/启动地址和量产行为。

## Keil 兼容策略

- 没有删除或替换 `CommomBQ769x0_16series_030C8T6_C.uvprojx`
- 没有改动 Keil 的目录结构
- GCC 启动文件、链接脚本、构建产物全部放在新增目录中
- 目标是“新增一条自动化工具链”，不是“替换 Keil”

## AI 后续接管建议

后续 AI 进入仓库后，建议按下面顺序工作：

1. 先执行 `git status --short`
2. 再执行 `task doctor`
3. 如未安装 Python 依赖，执行 `task init`
4. 构建时优先执行 `task build`
5. 需要看体积热点时执行 `task map`
6. 修改保护阈值、Flash 布局、校准常量前先 review

## 已知边界

- 这次迁移的重点是固件 GCC 构建链，不包含主机侧仿真
- 旧项目已有未提交改动，提交时应只纳入本次工具链迁移相关文件
- 如果后续需要把烧录、调试、日志采集也统一进 `Taskfile.yml`，可以继续沿用当前结构追加
