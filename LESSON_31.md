# LESSON 31 · UART 逐字节接收 + 组帧解析

> 学习目录：`C:\Users\fengjianhua\Desktop\learning\lesson_01`
> 本次目标（一句话）：把"一次性拿到三字节数组"改成"一个字节一个字节地收，收满一帧再解析"。
> 这是真实 UART 中断服务函数的工作方式，也是后面环形缓冲区、状态机的地基。

---

## 0. 文件安排

新建 `uart_rx_test.c`，**不要动 `main.c`**。

理由：`main.c` 已被 `.vscode/tasks.json` 固定为构建入口，里面还有 `getchar()` 等待输入。
练习放进独立文件，互不干扰，也符合"练习与工程隔离"。

---

## 1. Q1 批改：正确 ✅

**你的答案**：`count < sizeof(buffer) / sizeof(buffer[0])`，理由是"更好的移植性"。

方向对，但"移植性"只是表象，本质是两条：

### 1.1 消除魔数

`3` 和 `buffer[3]` 是两处独立的"事实"。改一处忘一处就是越界。
用 `sizeof` 让容量只有一个来源。

### 1.2 和元素类型解耦

| 写法 | 含义 | 换元素类型后 |
|---|---|---|
| `sizeof(buffer) / sizeof(buffer[0])` | 元素个数 | 依然正确 |
| `sizeof(buffer)` | 字节数 | 只有元素 1 字节时才等于个数 |

**扣分点**：回答里没提"为什么单独用 `sizeof(buffer)` 不够"。
若 `buffer` 改成 `uint16_t buffer[3]`，`sizeof(buffer)` = 6，你会写出 `count < 6`，
然后往只有 3 个元素的数组里写 6 次 —— 典型的"编译通过、运行越界"。

### 1.3 更彻底的写法（本课不必改，先知道）

```c
#define RX_BUF_SIZE 3
uint8_t buffer[RX_BUF_SIZE];
```

容量有名字，声明处和检查处共用同一个宏。以后改成 128 只改一个地方。
这也是 STM32 环形缓冲区的标准做法。

---

## 2. Q2 讲解：size_t 塞进 uint16_t 会发生什么

### 2.1 类型范围

| 类型 | 位宽（本机 MinGW x86-64，实为 32 位构建） | 范围 |
|---|---|---|
| `size_t` | 32 位（64 位平台上为 64 位） | 0 ~ 4294967295 |
| `uint16_t` | 16 位 | 0 ~ 65535 |

C 的隐式转换规则：**赋值/传参时按目标类型截断**（取低位），不是报错，也不是饱和到 65535。

### 2.2 实测输出（真实编译运行结果）

```text
sizeof(size_t) = 4          ← 这套 MinGW 是 32 位构建
UINT16_MAX     = 65535
count outside  = 65539
len (inside)   = 3          ← 65539 变成 3
```

源码：

```c
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

static void show_len(uint16_t len)
{
    printf("  len (inside)  = %u\n", (unsigned)len);
}

int main(void)
{
    size_t big = 65536u + 3u;

    printf("sizeof(size_t) = %zu\n", sizeof(size_t));
    printf("UINT16_MAX     = %u\n", (unsigned)UINT16_MAX);
    printf("count outside  = %zu\n", big);
    show_len(big);

    return 0;
}
```

### 2.3 三条结论

**结论一：不是"钳到 65535"，是取低 16 位。**
65539 mod 65536 = 3。传入的"长度 65539"在函数里变成"长度 3"，编译器一声不响。

**结论二：`-Wall -Wextra -Werror` 抓不到它。**
必须显式加 `-Wconversion` 才会报：

```text
warning: conversion to 'uint16_t' from 'size_t' may alter its value [-Wconversion]
```

这就是"严格编译通过 ≠ 类型安全"的具体例子。

**结论三：现在安全，但要知道边界在哪。**
三字节帧的 `count` 永远在 0–3，远小于 65536，当前调用点无害。
它变成真问题的条件只有一个：**某个调用点要传的字节数 ≥ 65536**。

播放器项目正好踩在边上：32 kHz × 16 bit × 双声道，几秒钟的 PCM 就上万字节。
若某天按"整块"传，`len` 被截成小值，表现为"只处理了开头一小段数据"：
音频断音、SPI 收包不完整。**编译零警告、逻辑看起来没问题，极难查。**

### 2.4 三种处理方式

1. 接口改 `size_t` —— 最干净，PC 和 MCU 上 `size_t` 都够装。
2. 保留 `uint16_t`（协议层长度本来就该 ≤ 65535，MCU 上还省寄存器），调用前先做范围检查。
3. 构建参数加 `-Wconversion`，让编译器长期把关 —— 代价是老代码冒一批警告。

**本课建议走第 2 种的思路**：知道这里在窄化，且它当前安全。
不改 `check_command` 签名（它是已验收接口，改签名要同步 `command.h` 和所有调用点，
属于接口变更），留到做模块化那一课统一动。

---

## 3. Q3 批改：一半对，方向偏了

**"不能输出 `Command`" —— 对。**
`count` 停在 2，`count == 3` 不成立，`check_command` 根本不会被调用。

**"要检查尾帧再输出" —— 不是本题的缺口。**
尾帧校验（`buf[2] == 0x55`）在 `check_command` 里已经做了，第 30 课就写好了。

**真正的缺口：程序会输出 2 行 `Received`，然后静默退出。**
没有 `Frame complete`，没有 `Command`，也没有任何"这帧没收满"的提示。

放进真实 UART 场景更糟：下一帧的 `0xAA` 会写进 `buffer[2]`，再下一个字节继续往后写……
**一个半包把后面所有帧全部错位**，然后表现为"明明发了正确命令却返回 0"。

由此引出后面三课：

1. 帧没到齐怎么办？（丢弃？保留？）
2. 字节之间隔了 100 ms 还算同一帧吗？（超时复位）
3. 帧头不对时，什么时候复位 `count`？（状态机）

---

## 4. 任务：拆成两步写

### 第 31A 步

只写接收循环，输出必须是这 3 行，暂时**不调用** `check_command`：

```text
Received: 1 bytes
Received: 2 bytes
Received: 3 bytes
```

骨架（只有注释，语句自己填）：

```c
for (size_t i = 0; i < sizeof(received) / sizeof(received[0]); i++)
{
    // 1) 缓冲区还能不能写？不能写就跳过
    // 2) 写入 buffer[?]
    // 3) count 加一
    // 4) 打印 Received
}
```

实现约束：

- 用 `sizeof` 算长度，不要写死 `3`。
- 顺序有讲究：先判断能不能写 → 再写 → 再 `count++` → 再打印。
- 打印在 `count` 更新**之后**，这样"每收到一个字节"才可见。
- `printf` 里 `size_t` 用 `%zu`，不能用 `%d`。

### 第 31B 步

A 跑通后再加：`count == 3` 时打印 `Frame complete`，
调用 `check_command(buffer, count)`，打印 `Command: %d`。

约束：

- `Command:` 后的数字必须来自 `check_command()` 返回值，**不能自己写 `1`**。
- `-Wall -Wextra -Werror` 下必须零警告。

### 需要自己判断的设计点

**第 5 步的判断放循环体内还是循环体外？**

两种写法在本课输入下输出一样，但只有一种能在"第 4 个字节又来"时天然挡住。
选循环内也行，但提交时要说明为什么。

---

## 5. 验证方法

```bash
gcc -std=c11 -Wall -Wextra -Werror -g -fexec-charset=UTF-8 uart_rx_test.c command.c -o uart_rx_test.exe
./uart_rx_test.exe
echo "exit code: $?"
```

预期输出（逐行比对，不能"差不多"）：

```text
Received: 1 bytes
Received: 2 bytes
Received: 3 bytes
Frame complete
Command: 1
```

验收分四档，别混：

| 改动 | 通过标准 | 说明 |
|---|---|---|
| 编译 | 零警告 | 只是语法和类型过关 |
| 原始输入 | 恰好 5 行 | 逻辑过关 |
| `{0xAA,0x02,0x55}` | `Command: 2` | 说明没写死 |
| `{0xAA,0x09,0x55}` | `Command: 0` | 无效帧路径通 |

四条都过，本课才算 L3。

---

## 6. 提交清单

1. `uart_rx_test.c` 全文
2. 编译命令的实际输出（零警告）
3. 三次运行的输出：`{0xAA,0x01,0x55}`、`{0xAA,0x02,0x55}`、`{0xAA,0x09,0x55}`
4. 第 5 步放循环内/外的理由，一句话

导师重点检查：数组边界、`count` 类型与格式符是否匹配、判断顺序、
`check_command` 返回值有没有被丢掉、有没有硬编码。

---

## 7. 对既有代码的审查意见（只记着，不改）

`main.c` 第 60 行 `getchar()` 是为了双击运行时不闪退，
但它让程序无法在脚本里自动跑完 —— `$LASTEXITCODE` 那一套就断了。
以后做自动化测试应拿掉，改用 VS Code 的 `"console": "integratedTerminal"`。

---

## 8. 学习记录 #31（进行中）

- 本次内容：UART 逐字节接收缓冲区；`size_t → uint16_t` 窄化实测。
- 实际完成：Q1 正确；Q2 已讲解并实测（65539 → 3，`-Wconversion` 才报警）；Q3 部分正确。
- 掌握等级：容量检查 L3；类型窄化 L1→L2 待练；半包/状态机 L1。
- 错误与薄弱点：把"帧尾校验"当成半包问题的答案；对隐式窄化无感知。
- 下一项任务：完成 31A/31B，提交代码与三次运行结果。
