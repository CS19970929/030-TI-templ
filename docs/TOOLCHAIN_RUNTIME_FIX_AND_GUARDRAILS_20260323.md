# 工具链运行问题修复与防再发说明

## 结论

当前 `GCC/CMake/Task` 工具链已经达到和 `Keil` 并行可用的状态：

- `Keil` 编译下载可正常运行
- `GCC` 编译下载可正常运行
- `VS Code + ST-Link/OpenOCD` 调试链路可继续使用
- `task build` / `task build-debug` / `task flash` 作为统一主入口可用

这次问题不是单点错误，而是几个“镜像布局 + 启动时序 + Preset 覆盖”叠加造成的。后面如果只修一个点，很容易再次复发。

## 这次实际修了什么

### 1. 修正了 GCC 预设没有真正应用“板级安全参数”的问题

相关文件：

- [CMakePresets.json](E:\TODO\030 + TI\CMakePresets.json)
- [firmware/CMakeLists.txt](E:\TODO\030 + TI\firmware\CMakeLists.txt)

修复内容：

- `firmware-release` 改为 `APP_ENABLE_LTO=ON`
- `firmware-release` 改为 `APP_OPT_LEVEL=s`
- `firmware-debug` 改为 `APP_ENABLE_LTO=ON`
- `firmware-debug` 改为 `APP_OPT_LEVEL=s`

原因：

- 之前虽然 `firmware/CMakeLists.txt` 已经往“更接近 Keil 的配置”调整，但 `CMakePresets.json` 仍在覆盖这些默认值。
- 结果是实际构建仍按更保守、体积更大的参数编译，导致镜像大小和布局继续偏离 `Keil`。

### 2. 修正了应用区 Flash 长度

相关文件：

- [firmware/linker/stm32f030c8_bq769x0_app.ld](E:\TODO\030 + TI\firmware\linker\stm32f030c8_bq769x0_app.ld)

修复内容：

- `FLASH ORIGIN` 保持为 `0x08001C00`
- `FLASH LENGTH` 从 `0xE400` 改为 `0xD400`

原因：

- 项目本身已经定义了保留区：
  - `0x0800F000`
  - `0x0800F400`
  - `0x0800F800`
  - `0x0800FC00`
- 所以应用区真实可用范围只能到 `0x0800EFFF`
- 即 `0x0800F000 - 0x08001C00 = 0xD400`

影响：

- 避免 GCC 产物踩到参数区、唤醒标志区、升级标志区、睡眠标志区
- 这是“GCC 下载后不正常运行”的核心风险之一

### 3. 修正了应用初始栈顶和 RAM 顶部保留区

相关文件：

- [firmware/linker/stm32f030c8_bq769x0_app.ld](E:\TODO\030 + TI\firmware\linker\stm32f030c8_bq769x0_app.ld)
- [Taskfile.yml](E:\TODO\030 + TI\Taskfile.yml)
- [firmware/CMakeLists.txt](E:\TODO\030 + TI\firmware\CMakeLists.txt)

修复内容：

- `_estack` 固定为 `0x20001B80`
- 主 RAM 改为 `0x200000C0 ~ 0x20001B7F`
- 新增 `RAM_TOP_RSVD = 0x20001B80 ~ 0x20001FFF`
- 默认 `APP_HEAP_SIZE` 从 `0x200` 改为 `0x0`

原因：

- `Keil` 镜像首向量首字就是 `0x20001B80`
- GCC 之前给到更高的栈顶，和 `Keil` 实际布局不一致
- 旧项目是带 bootloader/IAP 的，首向量和 RAM 顶部布局不一致时，启动稳定性会明显变差

### 4. 修正了 ELF 段布局，避免调试器按 ELF 段错误烧写

相关文件：

- [firmware/CMakeLists.txt](E:\TODO\030 + TI\firmware\CMakeLists.txt)

修复内容：

- 增加 `-Wl,--nmagic`
- 增加 `-nostartfiles`
- 丢弃 unwind / init array / fini array 等不需要段

原因：

- 之前 GCC ELF 的 `PT_LOAD` 可能被组织成不利于 bootloader 项目的段布局
- 这类项目如果下载器/调试器按 ELF 段烧写，而不是按 `bin + address` 烧写，容易擦到不该动的区域

当前确认结果：

- 现在 `readelf -l` 的首个 `LOAD` 段从 `0x08001C00` 开始

### 5. 修正了启动文件的向量表 remap 时序

相关文件：

- [firmware/startup/startup_stm32f0xx_gcc.S](E:\TODO\030 + TI\firmware\startup\startup_stm32f0xx_gcc.S)

修复内容：

- 在 `Reset_Handler` 最前面，先把应用向量表复制到 `0x20000000`
- 立即打开 `SYSCFG` 时钟
- 立即做 `SRAM remap`
- 然后再进入常规 `.data/.bss` 初始化、`SystemInit`、`main`

原因：

- `STM32F030` 没有 `VTOR`
- 这个项目又是 `bootloader + app` 结构
- 如果 app 跳转后第一批异常/中断发生得太早，而向量表还没 remap 到 SRAM，就会掉回 bootloader 的异常向量
- 这类现象表面看起来很像“app 没启动”或“下载后不运行”

这是这次能稳定跑起来的关键修复之一。

### 6. 让启动相关代码在链接顺序上更靠前、更接近 Keil

相关文件：

- [firmware/CMakeLists.txt](E:\TODO\030 + TI\firmware\CMakeLists.txt)
- [firmware/linker/stm32f030c8_bq769x0_app.ld](E:\TODO\030 + TI\firmware\linker\stm32f030c8_bq769x0_app.ld)

修复内容：

- 启动汇编文件放到 `FIRMWARE_SOURCES` 最前
- 在 `.text` 前显式 `KEEP(*(.text.Reset_Handler))`
- 在 `.text` 前显式 `KEEP(*(.text.Default_Handler))`

原因：

- 这能让应用头部布局更接近 `Keil`
- 也降低“入口符号虽然对，但布局和调试器/bootloader预期不一致”的风险

## 现在已经确认通过的检查

已完成：

- `task test` 通过
- `task build` 通过
- `task build-debug` 通过
- `GCC release ELF` 首个 `PT_LOAD` 段起始地址为 `0x08001C00`
- `GCC release bin` 首向量栈顶为 `0x20001B80`
- `GCC release bin` 未越过 `0x0800F000` 保留区
- 当前实机现象已确认：`GCC` 下载后可正常运行，`Keil` 也保持可用

当前关键数据：

- `Keil bin size = 39204`
- `GCC release bin size = 44032`
- `Keil flash_end = 0x0800B524`
- `GCC flash_end = 0x0800C800`

说明：

- GCC 当前仍比 Keil 大，但已经在安全范围内，没有踩保留区
- “比 Keil 大”本身不是问题，前提是地址、段布局、启动时序都正确

## 这次根因归纳

根因不是“GCC 编译器不行”，而是下面 4 类问题叠加：

1. `Preset` 覆盖了本来已经修过的安全参数
2. 链接脚本把应用可用 Flash 算大了，存在踩保留区风险
3. 首栈值和 RAM 顶部布局与 Keil 不一致
4. 启动阶段的 SRAM 向量表 remap 太晚，不适配 `STM32F030 + bootloader` 结构

只要其中任意两项同时存在，就很容易出现：

- 下载成功但不运行
- 调试可进 `main`，真实复位却不正常
- Keil 正常、GCC 异常

## 这次检查到的残留问题

当前没有再发现会阻塞“编译/下载/运行”的工具链问题，但还存在几项代码质量风险：

### 1. 业务代码编译告警仍然存在

主要在：

- [Sci_Upper.c](E:\TODO\030 + TI\Code\Source\Sci_Upper.c)
- [bsp_cpu_flash.c](E:\TODO\030 + TI\Code\Source\bsp\bsp_cpu_flash.c)

具体包括：

- 隐式声明导致的函数类型冲突
- 非 `void` 函数缺少返回值
- 局部变量可能未初始化就返回

这些现在没有阻塞运行，但属于“以后可能再次在 GCC 下放大成真实故障”的风险点，建议后续单独清理。

### 2. 旧文档和历史排障记录不能直接当最终事实

后续以本文件和当前仓库实际配置为准，不再以旧的临时排障记录作为最终依据。

## 以后必须执行的防再发规则

以后任何旧项目迁移到这套工具链，必须固定执行下面检查，不能跳：

1. 先确认 `Flash app origin / reserved flash / stack top`
2. 再确认 `CMakePresets.json` 没有把安全参数覆盖掉
3. 检查 `readelf -l`，首个 `LOAD` 必须从应用起始地址开始
4. 检查 `bin` 首向量：
   - `MSP`
   - `Reset_Handler`
   - `HardFault_Handler`
5. 检查 `bin` 最终大小没有越过保留区
6. 对于 `STM32F0 + bootloader + app` 项目，必须尽早完成 SRAM 向量表 remap
7. 构建通过后，必须做一次真实下载运行验证，不能只看“能进调试器”

## 推荐的固定回归命令

日常最小回归：

```powershell
task doctor
task test
task build
task build-debug
```

发布前回归：

```powershell
task doctor
task test
task build
task build-debug
task flash
```

如果后续再迁移别的老项目，建议优先使用模板仓库中的通用工具：

- `E:\TODO\030 + 309\scripts\bootstrap_legacy_keil.py`
- `E:\TODO\030 + 309\scripts\diagnose_legacy_boot.py`

前者用于一键补齐工具链，后者用于首向量、尺寸、ELF 段布局的静态诊断。

## 最终要求

以后不能再出现这类问题的前提，不是“以后不会出错”，而是：

- 迁移时必须带着地址边界和启动链路一起迁移
- 不能只迁 `build`，不迁 `startup/linker/preset`
- 不能只验证“能编译”，不验证“真实复位能运行”
- 不能只看 `CMakeLists.txt`，不看 `Preset` 最终生效值

这 4 条只要严格执行，后续同类问题基本都能在下载前暴露，而不是到板上才发现。
