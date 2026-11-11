# LEARNING_PROGRESS · C / STM32 嵌入式学习记录

> 目标：初级 MCU / 嵌入式软件工程师的独立开发能力。
> 本文件由导师维护，每次教学结束追加一条「学习记录」。
> 判定标准：**代码看起来正确 ≠ 编译成功 ≠ 运行成功 ≠ 测试覆盖充分**，四者分开标注。

## 1. 环境

| 项 | 值 |
|---|---|
| 学习目录 | `C:\Users\fengjianhua\Desktop\learning\lesson_01` |
| 编译器 | `C:\Users\fengjianhua\Documents\c\MinGW\bin\gcc.exe` |
| 严格参数 | `-std=c11 -Wall -Wextra -Werror -g` |
| IDE | VS Code（Ctrl+Shift+B）+ Keil5（STM32 侧） |
| 构建任务 | `.vscode/tasks.json`：`main.c + command.c → main.exe` |
| 日志语言 | 统一使用英文，避免 Windows 控制台 UTF-8 乱码干扰判断 |

## 2. 能力矩阵

| 模块 | 当前等级 | 依据 |
|---|---|---|
| C 基本语法、数组、循环、条件 | L3 | 多份练习独立编写并运行 |
| 函数 / 头文件 / 多文件工程 | L3 | `.c/.h` 分工、链接错误排查已实操 |
| 结构体与结构体数组 | L3 | `main.c` 用例表 + `struct_test.c` |
| 指针与缓冲区边界安全 | L2 | 会写 `NULL` 检查，`size_t`/`uint16_t` 混用隐患未评估 |
| 通信帧解析（定长三字节） | L3 | `check_command()` 完整实现 + 边界测试 |
| 逐字节流式接收 | L3 | 31A 独立写出循环、容量检查、索引与顺序全对 |
| 组帧 / 状态机 | L2 | 单帧 `count==3` 触发解析已通；引脚复位、多帧、半包仍是 L1 |
| GCC 编译与错误定位 | L3 | 编译错误、链接错误、逻辑错误均实际排查过 |
| 调试器断点 / 单步 | L1 | 未系统练习 |
| STM32 寄存器与标准库基础 | L1–L2 | 有接触，未独立驱动验证 |

## 3. 已完成且实际验证

- `check_command()`：`AA 01 55 → 1`、`AA 02 55 → 2`、其他 → 0。
- 入参防御：`buf == NULL`、`len < 3` → 0。
- 结构体数组用例表自动遍历，打印 `Expected / Actual / PASS / FAIL`。
- 通过数量统计 `Summary : 4/4 passed`，全通过 `return 0`，失败 `return 1`。
- PowerShell `$LASTEXITCODE` 验证退出码；故意改错期望值验证能报 FAIL。
- 严格参数下编译零警告通过。
- **31A（2026-11-10）**：独立实现逐字节接收循环（`uart_rx_test.c`），容量检查、
  `buffer[count]` 索引、`count++` 与打印顺序全部正确，编译零警告，输出
  `Received: 1/2/3 bytes`，exit code 0。
- **31B（2026-11-10）**：`count == 3` 时打印 `Frame complete`、调用
  `check_command(buffer, count)`、打印 `Command`。经导师独立复测三组输入：
  `0x01→1`、`0x02→2`、`0x09→0`，全部 exit code 0，编译零警告。
  已知瑕疵：第二参数一度误用 capacity（`sizeof`）而非 length（`count`），已改正。

## 4. 已知薄弱点（待强化）

1. `size_t count` 传给 `uint16_t len` 形参时的窄化风险未评估。
2. 缓冲区满后的策略（丢弃 / 覆盖 / 报错）尚未形成设计判断。
3. 不完整帧、错帧头、错帧尾的处理还没写过。
4. 调试器（断点、watch、单步）使用不熟练。
5. 把"AI 跑通了"与"我会了"分开：AI 辅助完成的部分不计入 L3。

## 5. 当前任务

**第 32 课 · 连续多帧接收（帧间复位）**（状态：待学员提交代码）

- 背景：31B 实测暴露——输入 4 个字节时，第 4 个字节被**静默丢弃**（silently dropped，
  无任何提示）。真实串口里丢一个字节，后面整条数据流都会错位（misalignment）。
- 任务：改 `uart_rx_test.c`，输入改成 6 字节 `{AA 01 55 AA 02 55}`，要求解析出两帧。
- 必须做到：一帧解析完成后**复位**（reset，把 `count` 归零），后续字节组成新的一帧。
- 预期输出：

```text
Received: 1 bytes
Received: 2 bytes
Received: 3 bytes
Frame complete
Command: 1
Received: 1 bytes
Received: 2 bytes
Received: 3 bytes
Frame complete
Command: 2
```

- 需要自己判断：① 复位语句放在哪一行？② 只把 `count` 归零够不够，`buffer` 里的旧数据要不要清？
- 验收：零警告编译 + 上述输出逐行比对 + 再测一组 `{AA 02 55 AA 09 55}`（预期 `2` 然后 `0`）。

## 6. 下一步路线

阶段一（PC 端通信基础）：逐字节接收 → 缓冲区满策略 → 连续多帧 → 半包/错帧处理 → 状态机 → 结构体保存解析状态 → 模块化拆分 → 异常输入自动化测试。

阶段二（STM32）：GPIO/按键 → USART 收发 → 接收中断 → 环形缓冲区 → SPI → 定时器中断 → DMA → DAC。

阶段三（系统能力）：模块接口、数据流、实时性分析、错误处理与日志、FreeRTOS、播放器卡顿定位、面试讲解。

## 7. 学习记录

### 学习记录 #31 · 已完成（31A + 31B）

- 本次内容：UART 逐字节接收缓冲区模拟，组帧后调用 `check_command()`。
- 实际完成：31A 独立写出接收循环；31B 加上「满帧即解析」。导师独立复测三组输入
  全部通过（`1 / 2 / 0`），编译零警告，exit code 0。
- 修掉的问题（6 + 1）：`<cstdint>` 应为 `<stdint.h>`、元素类型 `uint16_t`→`uint8_t`、
  缓冲区预填数据→`{0}`、`int count`→`size_t`、`main()` 补返回类型、全局变量→局部变量；
  31B 又把第二参数从 capacity（容量）改成 length（长度）。
- 掌握等级：接收循环与容量检查 **L3**；单帧组帧 **L2**；类型/签名细节 **L2**
  （7 个问题里 6 个靠批改才发现）。
- 错误与薄弱点：`<cstdint>` 当 C 头文件；用 `uint16_t` 存串口字节；缓冲区预填；
  变量习惯写全局；`main()` 省略 `(void)`；曾把 capacity 当 length 传参。
- 概念收获：窄化转换（narrowing conversion）取低位而非饱和；编译期常量能骗过
  `-Wconversion`；编译通过 ≠ 类型安全 ≠ 语义正确。
- 决定：**暂不把 `-Wconversion` 加进 `tasks.json`**，改为需要时手动
  `gcc -Wconversion -fsyntax-only` 体检。
- 下一项任务：第 32 课 连续多帧接收 + 帧间复位。
