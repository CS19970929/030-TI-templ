# 030 + TI 调试、诊断与自动化接管说明

## 1. 当前工具链怎么调试

当前项目已经具备三条调试路径：

- Keil 原生调试
- VS Code + Cortex-Debug + ST-Link/OpenOCD
- 命令行 OpenOCD + `arm-none-eabi-gdb`

其中推荐主路径是：

- 日常开发：VS Code 一键调试
- 深入定位：命令行 `gdb`
- 对照验证：Keil

## 2. VS Code 怎么调试

### 2.1 需要的环境

- 已安装 VS Code
- 已安装扩展 `Cortex-Debug`
- 已安装 Arm GNU Toolchain
- 已安装 OpenOCD
- 已连接 ST-Link
- 目标板已上电

### 2.2 当前已经配置好的文件

- [launch.json](E:\TODO\030 + TI\.vscode\launch.json)
- [tasks.json](E:\TODO\030 + TI\.vscode\tasks.json)
- [extensions.json](E:\TODO\030 + TI\.vscode\extensions.json)

### 2.3 直接使用方法

1. 打开 `E:\TODO\030 + TI`
2. 按 `F5`
3. 选择：
   - `STM32F030 Debug (ST-Link/OpenOCD)`
   - 或 `STM32F030 Debug (J-Link)`
4. VS Code 会自动先执行 `task build-debug`
5. 然后启动 OpenOCD 或 J-Link GDB Server
6. 载入 ELF
7. 停在 `main`

### 2.4 适合什么场景

- 下断点
- 单步
- 看变量
- 看调用栈
- 看寄存器

## 3. 命令行怎么调试

这是我这次实际用来定位 GCC 固件问题的方法。

### 3.1 先构建 Debug 固件

```powershell
task build-debug
```

输出 ELF 在：

- `E:\TODO\030 + TI\artifacts\cmake\firmware-debug\firmware\CommomBQ769x0_16series_030C8T6_C.elf`

### 3.2 启动 OpenOCD

```powershell
C:\Users\Administrator\AppData\Local\Microsoft\WinGet\Packages\xpack-dev-tools.openocd-xpack_Microsoft.Winget.Source_8wekyb3d8bbwe\xpack-openocd-0.12.0-7\bin\openocd.exe ^
  -s C:\Users\Administrator\AppData\Local\Microsoft\WinGet\Packages\xpack-dev-tools.openocd-xpack_Microsoft.Winget.Source_8wekyb3d8bbwe\xpack-openocd-0.12.0-7\openocd\scripts ^
  -f interface/stlink.cfg ^
  -f target/stm32f0x.cfg
```

OpenOCD 默认会监听：

- GDB 端口：`3333`

### 3.3 用 GDB 接入

```powershell
C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin\arm-none-eabi-gdb.exe ^
  E:\TODO\030 + TI\artifacts\cmake\firmware-debug\firmware\CommomBQ769x0_16series_030C8T6_C.elf
```

进入 `gdb` 后常用命令：

```gdb
target extended-remote localhost:3333
monitor reset halt
load
thbreak main
continue
break HardFault_Handler
continue
bt
info registers
```

## 4. 我这次是怎么调试的

这次我没有只停留在“构建成功”，而是继续做了在线调试。

### 4.1 已经确认的事实

1. `task build` 可以成功
2. `task flash` 可以通过 ST-Link/OpenOCD 正常下载
3. 当前 GCC 固件下载后“不能正常运行”
4. Keil 编译版“可以正常运行”

### 4.2 我实际执行过的关键动作

1. 用 `task flash` 实际下载 GCC 版固件
2. 对比了 Keil 与 GCC 的 `bin` 大小
   - Keil：约 `39204` 字节
   - GCC：约 `54468` 字节
3. 对比了 Keil map 和 GCC map
4. 检查了：
   - 向量表
   - `Init_IAPAPP`
   - `VectorTable`
   - RAM 重映射
   - `Reset_Handler`
5. 启动 OpenOCD
6. 用 `arm-none-eabi-gdb` 连接并 `load`
7. 在 `main` 打临时断点并运行

### 4.3 当前定位到的结论

当前 GCC 固件：

- 不是“下载地址错误”
- 不是“复位后一上来就飞掉”
- 不是“进不了 `main`”

我已经确认：

- GCC 版程序可以成功停在 `main()`

这说明问题已经缩小到：

- `InitDevice()`
- `InitVar()`
- 或主循环前后某个初始化/运行阶段

也就是说，现在问题更像是：

- GCC 与 Keil 的运行时行为差异
- 旧代码里的未定义行为在 GCC 下被放大
- 初始化顺序/时钟/中断/重映射/内存访问细节没有完全对齐

### 4.4 当前还没完全闭环的地方

我已经把问题从“不能跑”缩小到了“进入 main 之后某处出问题”，但这一轮还没有把最终故障点完全抓到。

下一步最有效的方法是：

1. 在 `InitDevice`
2. 在 `Init_IAPAPP`
3. 在 `Init_RTC`
4. 在 `InitAFE1`
5. 在 `TIM17_IRQHandler`
6. 在 `HardFault_Handler`

逐步断点推进，找到第一处异常。

## 5. 以后怎么自动化帮你定位问题

可以做，而且应该做。

目标不是只做“构建脚本”，而是做一套“AI 可接管的诊断工具链”。

## 6. 建议做的自动化工具

建议拆成三类工具。

### 6.1 迁移工具

目标：

- 给老项目一键补齐 `CMake + Taskfile + Python + VS Code` 工具链

能力：

- 解析 `.uvprojx`
- 提取源文件、宏定义、包含目录
- 提取输出名、地址布局
- 生成：
  - `Taskfile.yml`
  - `CMakePresets.json`
  - `firmware/CMakeLists.txt`
  - `toolchains/*.cmake`
  - `scripts/*.py`
  - `docs/*.md`

### 6.2 诊断工具

目标：

- 自动帮你回答“为什么 GCC 版跑不起来”

能力：

- 对比 Keil / GCC 源文件清单
- 对比宏定义
- 对比 map 大小与分布
- 对比向量表
- 自动连 ST-Link
- 自动运行 OpenOCD + GDB
- 自动抓：
  - 当前 PC
  - MSP/PSP
  - xPSR
  - HardFault 停点
  - 调用栈
- 自动生成诊断报告

### 6.3 接管工具

目标：

- 帮你接管重复劳动

能力：

- 自动构建
- 自动下载
- 自动调试连接
- 自动采集故障现场
- 自动汇总差异报告
- 自动生成建议修改点

## 7. 迁移到旧项目的工具现在做好了吗

结论：还没有正式做完，当前还是“做到一半”。

我之前已经明确了方案，也开始动手写了脚本思路，但还没有形成一版你现在就能直接稳定使用的正式工具。

所以现在不能告诉你“已经好了，直接用某条命令就行”。

目前真实状态是：

- 方案明确了
- 目标明确了
- 当前 `030 + TI` 的这次迁移是人工迁移 + 规则固化
- 但“通用一键迁移脚本”还没完成交付

## 8. 这个迁移工具做完后会怎么用

计划中的使用方式应该是这样：

```powershell
py -3.12 scripts/bootstrap_legacy_keil.py --project-root "E:\TODO\某旧项目"
```

或者通过统一入口：

```powershell
task bootstrap-legacy LEGACY_ROOT=E:/TODO/某旧项目
```

脚本完成后会自动生成：

- 工具链文件
- VS Code 调试配置
- 基础文档
- 可执行命令入口

然后你进入旧项目直接执行：

```powershell
task doctor
task build
task flash
```

## 9. 当前建议

当前最优先的不是继续扩展文档，而是先把这次 GCC 版“进入 main 后异常”的具体点抓出来。

建议下一步直接做：

1. 用 ST-Link/OpenOCD/gdb 在 `InitDevice` 内逐步断点
2. 抓到第一次异常点
3. 修正 GCC / Keil 差异
4. 固化为诊断脚本

这样后面的迁移工具和自动化接管工具才会更靠谱。
