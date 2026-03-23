# 030 + TI 工具链接管说明

## 当前状态

当前工具链迁移已经补齐到可日常使用的程度，包含四类能力：

- GCC/CMake 构建
- J-Link / OpenOCD 烧录
- VS Code 一键调试
- 面向 AI 的统一命令入口

原有 Keil 工程仍保留，可继续使用；新增工具链不是替换 Keil，而是补一条可脚本化、可自动化、可被 AI 接管的主流程。

## 你现在可以直接用的命令

### 环境检查

- `task doctor`
  - 检查 `python`、`uv`、`cmake`、`ninja`、`arm-none-eabi-gcc`、`JLink`、`JLinkGDBServerCL`、`openocd`
- `task init`
  - 初始化 Python 工具环境
- `task test`
  - 检查本仓库自动化脚本入口

### 构建

- `task build`
  - 构建 Release 固件
- `task build-debug`
  - 构建 Debug 固件
- `task rebuild`
  - 清理后重新构建 Release 固件
- `task rebuild-debug`
  - 清理后重新构建 Debug 固件
- `task clean`
  - 清空 `artifacts/`
- `task map`
  - 分析 map 文件热点

### 烧录

- `task flash`
  - 默认走 ST-Link + OpenOCD，烧录 Release 固件
- `task flash-stlink`
  - 使用 ST-Link + OpenOCD 烧录 Release 固件
- `task flash-jlink`
  - 使用 J-Link 烧录 Release 固件
- `task flash-jlink-debug`
  - 使用 J-Link 烧录 Debug 固件

### VS Code 调试

- 打开 VS Code 后按 `F5`
- 如果你平时用 ST-Link，选择 `STM32F030 Debug (ST-Link/OpenOCD)`
- 如果你平时用 J-Link，选择 `STM32F030 Debug (J-Link)`
- 调试前会自动执行 `task build-debug`

前提：

- 已安装 VS Code 扩展 `Cortex-Debug`
- 已连接 J-Link
- 开发板上电
- SWD 连接正确

## 关键文件

- [Taskfile.yml](E:\TODO\030 + TI\Taskfile.yml)
  - 统一命令入口
- [CMakePresets.json](E:\TODO\030 + TI\CMakePresets.json)
  - GCC Debug/Release 预设
- [firmware/CMakeLists.txt](E:\TODO\030 + TI\firmware\CMakeLists.txt)
  - 固件 GCC 构建描述
- [firmware/linker/stm32f030c8_bq769x0_app.ld](E:\TODO\030 + TI\firmware\linker\stm32f030c8_bq769x0_app.ld)
  - 链接地址布局
- [firmware/startup/startup_stm32f0xx_gcc.S](E:\TODO\030 + TI\firmware\startup\startup_stm32f0xx_gcc.S)
  - GCC 启动文件
- [scripts/check_toolchain.py](E:\TODO\030 + TI\scripts\check_toolchain.py)
  - 工具链巡检
- [scripts/flash_jlink.py](E:\TODO\030 + TI\scripts\flash_jlink.py)
  - J-Link 烧录脚本
- [scripts/flash_openocd.py](E:\TODO\030 + TI\scripts\flash_openocd.py)
  - ST-Link/OpenOCD 烧录脚本
- [.vscode/launch.json](E:\TODO\030 + TI\.vscode\launch.json)
  - VS Code 一键调试入口
- [.vscode/tasks.json](E:\TODO\030 + TI\.vscode\tasks.json)
  - VS Code 任务入口

## 固件产物位置

### Release

- ELF: [CommomBQ769x0_16series_030C8T6_C.elf](E:\TODO\030 + TI\artifacts\cmake\firmware-release\firmware\CommomBQ769x0_16series_030C8T6_C.elf)
- BIN: `E:\TODO\030 + TI\artifacts\cmake\firmware-release\CommomBQ769x0_16series_030C8T6_C.bin`
- HEX: `E:\TODO\030 + TI\artifacts\cmake\firmware-release\CommomBQ769x0_16series_030C8T6_C.hex`

### Debug

- ELF: `E:\TODO\030 + TI\artifacts\cmake\firmware-debug\firmware\CommomBQ769x0_16series_030C8T6_C.elf`
- BIN: `E:\TODO\030 + TI\artifacts\cmake\firmware-debug\CommomBQ769x0_16series_030C8T6_C.bin`
- HEX: `E:\TODO\030 + TI\artifacts\cmake\firmware-debug\CommomBQ769x0_16series_030C8T6_C.hex`

## 怎么下程序

### 方案 1：ST-Link，推荐

1. 先执行 `task build`
2. 执行 `task flash` 或 `task flash-stlink`

### 方案 2：J-Link

1. 先执行 `task build` 或 `task build-debug`
2. 烧录 Release：`task flash-jlink`
3. 烧录 Debug：`task flash-jlink-debug`

如果你最常用的是 ST-Link，日常直接用 `task flash` 就行。

## 怎么调试

### 方案 1：VS Code 一键调试，推荐

1. 安装 `Cortex-Debug`
2. 打开本项目
3. 确认 ST-Link 或 J-Link 已连接
4. 按 `F5`
5. 选择对应配置
   - ST-Link：`STM32F030 Debug (ST-Link/OpenOCD)`
   - J-Link：`STM32F030 Debug (J-Link)`

它会自动：

- 执行 `task build-debug`
- 启动 OpenOCD 或 J-Link GDB Server
- 载入 Debug ELF
- 在 `main` 停下

### 方案 2：Keil

如果需要继续沿用旧流程，仍可直接打开：

- `CommomBQ769x0_16series_030C8T6_C.uvprojx`

## 内存布局约束

当前 GCC 链接脚本按 Keil 工程中的布局迁移，关键地址如下：

- Flash 起始地址：`0x08001C00`
- Flash 长度：`0xE400`
- RAM Vector 区：`0x20000000`，长度 `0xC0`
- 主 RAM 区：`0x200000C0`，长度 `0x1F40`
- 默认 Heap：`0x200`
- 默认 Stack：`0xC00`

不要在没有 review 的前提下直接修改这些值，因为它们关系到 IAP/启动地址和量产行为。

## 推荐日常流程

### 只编译

1. `task doctor`
2. `task build`

### 编译并下载

1. `task build`
2. `task flash`

### 进入在线调试

1. `task doctor`
2. 打开 VS Code
3. 按 `F5`

## 当前边界

- 这次迁移已覆盖构建、烧录、调试主流程
- 暂未补主机侧仿真
- 当前仓库里原本已有未提交业务改动，提交时仍应避免混入
- 若后续需要串口日志采集、自动 attach、运行时快照抓取，可以继续沿用当前结构追加
