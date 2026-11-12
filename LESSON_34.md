# 第 34 课 · 把接收逻辑拆成模块

## 0. 为什么必须模块化

你现在的代码，全部在 `main` 里：

```text
main()
 ├─ received[]      测试数据
 ├─ buffer[]        接收缓冲区
 ├─ count           接收进度
 └─ for 循环         逐字节喂数据
```

**真机上没有这个 `for` 循环。** STM32 里数据是硬件送来的，程序长这样：

```c
void USART1_IRQHandler(void)      // 中断服务函数，每收到一个字节就被调用一次
{
    uint8_t byte = USART_ReceiveData(USART1);
    uart_rx_feed_byte(byte);      // 只有这一行
}
```

中断函数里**不能有循环**、不能等数据、不能长时间占用 CPU。它只知道"来了一个字节"，处理完必须立刻返回。

所以接收逻辑必须长成"喂一个字节 → 处理一个字节"的形状，状态（缓冲区、接收进度）必须活在函数外面。**这就是模块化的根本原因，不是为了"好看"。**

---

## 1. 三个必须先决定的点

### 决策 1：`buffer` 和 `count` 放哪？（本课核心）

| 方案 | 写法 | 优点 | 缺点 |
|---|---|---|---|
| **A 全局变量** | 定义在 `.c` 里，不加 `static` | 最简单，函数不用改签名 | 任何文件都能改，出问题无法定位；无法同时存在两个接收通道 |
| **B 文件内 `static`** | `static uint8_t s_buffer[...]` | 外部拿不到，安全 | **无法测试**：`main` 读不到"收了几字节"，也读不到 `buffer` 内容 |
| **C 结构体 + 指针** | `uart_rx_feed_byte(&rx, byte)` | 状态可见、可测试、可多实例 | 每个调用都要传 `&rx`，函数内要写 `ctx->` |

**推荐 C**，三个理由：

1. **可测试**：`main` 能直接打印 `rx.count`，验证"这一帧收了几字节"。B 方案做不到，只能靠打印猜。
2. **可多实例**：你的播放器要同时跑 UART（收命令）和 SPI（收 PCM）。C 方案只要声明两个变量 `uart_rx_t uart_rx; uart_rx_t spi_rx;` 就各管各的。A/B 方案里 `buffer`/`count` 只有一份，两个通道会互相踩。
3. **零全局状态**：没有全局变量就没有"这个 `count` 是被谁改的"这类问题。多任务 / 中断 / 主循环混跑时，这一点是生死线。

这不是我编的。STM32 HAL 库所有外设函数都长这样：

```c
HAL_UART_Receive(&huart1, buf, len, timeout);   // huart1 是 UART_HandleTypeDef 结构体
```

`huart1` 里装着这个串口的全部状态（缓冲区、标志位、错误码）。Linux 驱动、FreeRTOS 任务句柄，全是同一个思路。

**代价**：函数内所有 `buffer` 要写成 `ctx->buffer`，`count` 要写成 `ctx->count`。多敲几个字符，换来上面三条。

### 决策 2：帧长常量放哪？

现在的 `#define FRAME_LEN 3` 在 `uart_rx_test.c` 里。模块化后它属于**接收模块**，应该搬到 `uart_rx.h`。

顺便把 `0xAA` / `0x55` 也提出来：

```c
#define RX_HEADER 0xAA
#define RX_TAIL   0x55
```

理由和 `FRAME_LEN` 一样：协议改了只改一处，而且 `0xAA` 出现在代码里 4 次，读者不知道哪个是"帧头"、哪个是"恰好等于 0xAA 的值"。

### 决策 3：`check_command()` 在哪调用？

三个选择：

| 位置 | 后果 |
|---|---|
| `uart_rx.c` 里（接收模块内部）| 接收模块依赖命令模块，**耦合**（coupling，两个模块绑在一起）。将来 STM32 上要换成"发消息给队列"，得改接收模块 |
| 用回调函数（callback，函数指针）| 最干净，但你还没学函数指针 |
| `main` 里 | 但 `main` 不知道"何时收满一帧" |

**本课（34A）先放 `uart_rx.c` 里**，接受这个耦合。理由是：先把"状态归属"这一件事改对，别同时改两件事。34B 再讨论怎么把应用层逻辑搬出去（返回值 / 回调 / 队列）。

---

## 2. 五步走

**第 1 步**：新建 `uart_rx.h`（接口契约，只有声明，没有实现）
**第 2 步**：新建 `uart_rx.c`（实现，把现有逻辑搬进来）
**第 3 步**：改 `uart_rx_test.c` 的 `main`，只负责"喂字节"
**第 4 步**：编译时要带上 `uart_rx.c`，否则链接报 `undefined reference`
**第 5 步**：六条用例输出必须**完全不变**（除了新增的 `final count` 行）

---

## 3. `uart_rx.h`（已为你创建，可直接用）

```c
#ifndef UART_RX_H
#define UART_RX_H

#include <stdint.h>
#include <stddef.h>

#define RX_FRAME_LEN 3
#define RX_HEADER    0xAA
#define RX_TAIL      0x55

typedef struct
{
    uint8_t buffer[RX_FRAME_LEN];
    size_t  count;
} uart_rx_t;

void uart_rx_init(uart_rx_t *ctx);
void uart_rx_feed_byte(uart_rx_t *ctx, uint8_t byte);

#endif
```

逐行说明：

| 部分 | 作用 |
|---|---|
| `#ifndef / #define / #endif` | 头文件保护（include guard），防止同一个头文件被包含两次导致重复定义 |
| `typedef struct { ... } uart_rx_t;` | 定义"一次接收会话"的完整状态。`typedef` 让你可以直接写 `uart_rx_t rx;` 而不用写 `struct uart_rx_t` |
| `uint8_t buffer[RX_FRAME_LEN]` | 帧缓冲区，长度用宏而不是 `3` |
| `size_t count` | 已经收到的字节数，类型和 `sizeof` 一致，避免 `sign-compare` |
| `void uart_rx_init(uart_rx_t *ctx)` | 初始化。**必须有**——结构体变量在栈上是未初始化的，`count` 可能是任意垃圾值 |
| `void uart_rx_feed_byte(uart_rx_t *ctx, uint8_t byte)` | 喂一个字节。这就是将来中断服务函数里唯一要调的东西 |

**为什么参数是 `uart_rx_t *ctx` 而不是 `uart_rx_t ctx`？**

C 语言只有值传递（pass by value）。传结构体本身会**复制整个结构体**（这里 16 字节），函数改的是副本，外面看不到变化。传指针，函数改的是原件。写接收逻辑必须改原件。

---

## 4. `uart_rx.c` 骨架（自己写实现）

```c
#include <stdio.h>
#include "uart_rx.h"
#include "command.h"

void uart_rx_init(uart_rx_t *ctx)
{
    // 1) 把 ctx->count 设为 0
    // 2) 把 ctx->buffer 全部清 0（用 for 循环，别写成三行硬编码）
}

void uart_rx_feed_byte(uart_rx_t *ctx, uint8_t byte)
{
    // 把 uart_rx_test.c 里 for 循环体内的四段逻辑搬到这里，
    // 逐条替换：
    //   received[i]  →  byte
    //   buffer       →  ctx->buffer
    //   count        →  ctx->count
    //   FRAME_LEN    →  RX_FRAME_LEN
    //   0xAA         →  RX_HEADER
    //   0x55         →  RX_TAIL
    //
    // ① 帧尾位置收到非法字节 → ctx->count = 0，打印 Invalid frame
    // ② 命令码位置收到帧头字节 → 空操作（假帧头，丢弃本字节）
    // ③ 帧头位置 → 开新帧
    // ④ 正常写入
    // ⑤ 收满 → 打印 Frame complete，调用 check_command，ctx->count 归零
}
```

**两条硬性约束：**

1. `uart_rx.c` 里**不允许出现** `0xAA`、`0x55`、`3` 这些裸数字，全部用 `uart_rx.h` 里的宏。
2. `buffer` 和 `count` **不允许**是文件级变量，必须是 `ctx->` 成员。

`uart_rx_init` 里清空 buffer 的写法，为什么不能用三行硬编码：

```c
// 差：帧长改成 5 就漏清两格
ctx->buffer[0] = 0; ctx->buffer[1] = 0; ctx->buffer[2] = 0;

// 好：跟着宏走
for (size_t k = 0; k < RX_FRAME_LEN; k++) { ctx->buffer[k] = 0; }
```

---

## 5. `main` 改成什么样

```c
#include <stdio.h>
#include <stdint.h>
#include "uart_rx.h"

int main(void)
{
    uint8_t received[] = {0xAA, 0xAA, 0x01, 0x55};
    // 其余用例注释在下面，切换时改这一行

    uart_rx_t rx;
    uart_rx_init(&rx);

    for (size_t i = 0; i < sizeof(received) / sizeof(received[0]); i++)
    {
        uart_rx_feed_byte(&rx, received[i]);
    }

    printf("final count: %zu\n", rx.count);   // C 方案的回报：状态可见

    return 0;
}
```

注意 `main` 里现在**看不到任何接收细节**了：没有 `buffer`、没有 `count`、没有 `check_command`。想知道"收了几字节"，读 `rx.count`。这就是模块化的效果——**接口暴露什么，调用方才能看到什么**。

`main.c` 里不要 `#include "command.h"` 了，因为 `main` 已经不直接调用它。**每个文件只包含自己真正用到的东西。**

---

## 6. 编译命令变了

以前：

```bash
gcc -std=c11 -Wall -Wextra -Werror -g -fexec-charset=UTF-8 uart_rx_test.c command.c -o uart_rx_test.exe
```

现在要加一个文件：

```bash
gcc -std=c11 -Wall -Wextra -Werror -g -fexec-charset=UTF-8 uart_rx_test.c uart_rx.c command.c -o uart_rx_test.exe
```

**漏掉 `uart_rx.c` 会怎样？** 你不会收到编译错误，而是**链接错误**（linker error）：

```text
undefined reference to `uart_rx_feed_byte'
```

这是你第 30 课踩过的坑：`#include` 只是把声明抄进来，**并不会自动编译对应的 `.c` 文件**。链接器必须看到函数的真正实现。

`.vscode/tasks.json` 里的 `args` 也要同步加上 `uart_rx.c`，否则 `Ctrl+Shift+B` 编不过。**这是改你的工程配置，我不会擅自动，你说要改我再改。**

---

## 7. 验收标准

| 项 | 要求 |
|---|---|
| 编译 | 三个 `.c` 一起编，零警告 |
| 六条用例 | 输出与第 33 课完全一致（`AA AA 01 55` → 三行 Received + `Command: 1`；`AA AA 55` → 无 `Frame complete`）|
| `final count` | 无完整帧时是当前进度（0/1/2），成帧后是 0 |
| 魔数自查 | `uart_rx.c` 里 grep 不到 `0xAA`、`0x55`、单独的数字 `3` |
| 状态自查 | `uart_rx.c` 里 grep 不到裸的 `count`（必须全是 `ctx->count`）|

```bash
grep -n "0xAA\|0x55" uart_rx.c        # 应该没有输出
grep -n "[^>]count" uart_rx.c         # 应该没有裸 count
```

---

## 8. 本课要能回答的问题（面试常问）

1. 为什么接收状态不能放在函数内部的局部变量里？
2. 为什么 `uart_rx_feed_byte` 的参数是指针而不是结构体本体？
3. 为什么 `uart_rx_init` 是必需的？不调用会怎样？
4. A / B / C 三种状态归属方案，你在什么场景下会选 B？

第 4 题没有标准答案，但你要能说出 B 的适用条件：**只有一个通道、不需要外部观测状态、且确定将来不会扩展到多通道**。
