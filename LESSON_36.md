# 第 36 课 · 接收超时（timeout）：半帧断流时如何复位

> 本课讲义已写好并**在本机实跑验证**（参考实现零警告，6 条验收用例输出逐字一致）。
> **实现代码由你自己写** —— 这是你的求职项目，抄讲义的代码进简历等于抄别人的项目。
> 讲义给你的是：**接口契约** + **为什么这样设计** + **怎么验证你写对了**。

---

## 0. 上一课留下的坑

第 35 课的三层协议栈已经能正确收帧。但有一个场景它完全没覆盖：

```text
上位机发了一帧的前半截，然后掉线 / 复位 / 拔线了
```

比如收到 `AA 06 A2` 之后对端就没声了。此时 `uart_rx.c` 的状态机停在 `RX_WAIT_DATA`，
`count = 3`，**永远等那 4 个数据字节**。

后果有三个：

1. **接收器永久卡死** —— 后面再来完整帧，字节被当成"上一帧的数据"吃掉，一帧都收不到。
2. **错误被掩盖** —— `err_count` 不会增加，因为格式上没出错，只是没收完。日志里看不出任何异常。
3. **缓冲区留脏数据** —— `buffer[0..2]` 永远是那半帧的残骸。

这在真实设备上是必现的：UART 对端复位、串口线松动、蓝牙断连、上位机软件崩溃——都会留下半帧。

**解法：超时复位（timeout）** —— 如果一帧在规定时间内没收完，就判定这帧废了，回到等帧头状态。

---

## 1. 三个关键决策

### 决策 1：超时判断放在哪一层？

| 方案 | 说明 | 评价 |
|---|---|---|
| A. 接收层内部自带计时 | `feed_byte` 里自己查时间 | ❌ 接收层不该知道"时间"这种东西，且单片机没有系统时钟 |
| B. **外部定时器调用检查函数** | 硬件定时器每隔 N ms 调一次 `uart_rx_check_timeout()` | ✅ **采用** |
| C. 主循环轮询 | while 循环里反复查 | ❌ 阻塞，且在中断模型下没有"主循环"这个概念 |

选 B。理由和第 35 课"接收层与解析层分层"是同一条逻辑：**接收层只管"怎么组帧"，不管"什么时候该放弃"**——后者是调度策略，属于调用方。

在 STM32 上的落法是：**SysTick 或一个 TIM 提供毫秒 tick，主循环每 10 ms 查一次**。

### 决策 2：用什么判断"半帧在途"？

这是本课最容易写错的地方，有两个候选：

```c
if (ctx->count == 0)      /* 方案一：用已收字节数 */
{
    return 0;
}
```

```c
if (ctx->state == RX_WAIT_HEADER)   /* 方案二：用状态机状态 */
{
    return 0;
}
```

**方案一是错的**，而且错得很隐蔽：

第 35 课的 `RX_WAIT_TAIL` 分支里，成帧时是这样写的：

```c
ctx->buffer[ctx->len - 1] = byte;
ctx->count = ctx->len;      /* 直接给帧长，不依赖"进来时正好是 len-1" */
ctx->state = RX_WAIT_HEADER;
if (byte == RX_TAIL)
{
    return 1;               /* 收到完整帧，count 保持为帧长，由调用方读取后复位 */
}
```

**成帧后 `count` 等于帧长（不是 0），要等调用方读完才调 `uart_rx_reset()`。**
在这段窗口里用 `count == 0` 判断，会把"刚刚好收完一帧"误判成"半帧断流"，
然后 `uart_rx_reset()` 把这一帧的 `buffer` 全部清零 —— **调用方读到的就是空数据**。

**必须用 `state`**：`state == RX_WAIT_HEADER` 精确表达"当前不在帧中间"。
成帧后 `state` 已经是 `RX_WAIT_HEADER`（即使 `count` 还是帧长），所以判定正确。

> 这是我在写参考实现时**实测踩到的**：`count == 0` 版本在"成帧后不立刻 reset"的边界用例上
> 误报超时并清空了 buffer。改用 `state` 后该用例输出 `fired=0`，buffer 完好。
> **验收用例 7 就是专门测这一条的**，你写完一定要跑。

### 决策 3：时间差怎么算才不怕溢出？

`uint32_t` 毫秒计数跑到 `0xFFFFFFFF` 会绕回 0。如果写成：

```c
if (now_ms > ctx->last_byte_tick + RX_TIMEOUT_MS)   /* ❌ 加法会溢出 */
```

`last_byte_tick + 100` 在接近回绕时会变成一个很小的数，判断就错了。

正确写法是**无符号减法**：

```c
if ((uint32_t)(now_ms - ctx->last_byte_tick) <= RX_TIMEOUT_MS)
{
    return 0;   /* 还没超时 */
}
```

无符号减法天然处理回绕：`(uint32_t)(a - b)` 在 `a < b` 时得到的是一个很大的正数，
恰好表示"间隔已经绕过去了很久"。**前提是两次检查的真实间隔小于 2^32 毫秒（约 49.7 天）**——
这在任何嵌入式系统里都成立。

**验收用例 6 专门测这个**：tick 从 `0xFFFFFF00` 起，跨过回绕点，两种写法结果必须一致。

---

## 2. 接口契约（你要实现的东西）

### 2.1 `protocol.h` 不动

超时阈值我建议放在 `protocol.h`（协议相关参数集中地）：

```c
#define RX_TIMEOUT_MS 100u /* 半帧断流超时阈值：两次字节之间允许的最大间隔 */
```

> **为什么是 100 ms？** 115200 波特率下一个字节 10 bit ≈ 87 µs，最长帧 16 字节也只要 1.4 ms。
> 100 ms 是**三个数量级**的余量——既能容忍主循环的调度抖动和中断延迟，
> 又不会让"对端卡住"拖太久。**这个阈值是权衡出来的，不是拍脑袋的**，
> 面试时能讲清楚"为什么不是 10 ms 也不是 10 s"就是加分项。

### 2.2 `uart_rx.h` 要加的字段

在 `uart_rx_t` 里加两个字段：

```c
uint32_t timeout_count;   /* 超时复位次数 */
uint32_t last_byte_tick;  /* 最后一次收到字节的时刻（毫秒） */
```

**为什么 `timeout_count` 要和 `err_count` 分开？**

它们是**性质完全不同的两类失败**：

| 计数器 | 含义 | 该怎么响应 |
|---|---|---|
| `err_count` | 对端发了**格式非法的数据** | 对端实现有 bug，要上报、要抓包 |
| `timeout_count` | 对端**没发完**（断流/掉线） | 链路问题，可能要重连、要告警 |

混在一起你就没法回答"到底是对方协议写错了，还是线松了"这个运维问题。

### 2.3 `uart_rx.h` 要加的函数

```c
/* 检查是否发生半帧断流超时。
 * 返回 1 = 发生了超时，接收状态已复位；返回 0 = 没超时。
 * now_ms 是当前时刻（毫秒），由调用方从硬件定时器拿。
 * 建议每 10 ms 调一次。 */
int uart_rx_check_timeout(uart_rx_t *ctx, uint32_t now_ms);
```

### 2.4 `feed_byte` 要加一个参数

```c
int uart_rx_feed_byte(uart_rx_t *ctx, uint8_t byte, uint32_t now_ms);
```

**为什么要加 `now_ms`？** 因为"最后一次收到字节的时刻"必须由喂字节的那一刻更新。
如果不在这里更新，就得再开一个 `uart_rx_mark_activity(ctx, now_ms)` 让调用方记得调——
**多一个"记得调用"的函数 = 多一个能忘的地方**。加参数更不容易漏。

> ⚠️ **这会改动 `uart_rx_test.c` 的调用**（第 35 课的测试文件）。
> 按你的工程约定，练习代码放独立文件、**不改 `main.c`** ——
> 所以**新建一个 `uart_rx_timeout_test.c`**，不要动 `uart_rx_test.c`。
> `main.c` / `command_test.c` / `struct_test.c` 都没调用 `feed_byte`，不受影响。

### 2.5 `uart_rx.c` 的三处改动

1. **`uart_rx_init`**：多清两个字段（`timeout_count = 0`、`last_byte_tick = 0`）。
2. **`uart_rx_feed_byte`**：函数体第一行更新 `ctx->last_byte_tick = now_ms;`
   （**在 while 循环外面**——重同步时同一个字节会被判两次，在循环里更新会多写一次，虽无害但语义不干净）。
3. **新增 `uart_rx_check_timeout`**：

```c
int uart_rx_check_timeout(uart_rx_t *ctx, uint32_t now_ms)
{
    /* 空闲状态（还在等帧头）不算超时 —— 那只是没人说话。
     * 注意用 state 判断而不是 count：成帧后 count 仍等于帧长（还没调 reset），
     * 用 count 会把"刚收完一帧"误判成"半帧断流"。 */
    if (ctx->state == RX_WAIT_HEADER)
    {
        return 0;
    }

    /* 用无符号减法做时间差比较：即使 tick 溢出回绕，
     * (uint32_t)(now - last) 在 2^32 毫秒内仍然是对的。 */
    if ((uint32_t)(now_ms - ctx->last_byte_tick) <= RX_TIMEOUT_MS)
    {
        return 0;
    }

    /* 超时：丢掉这半帧，回到等帧头状态。 */
    uart_rx_reset(ctx);
    ctx->timeout_count++;
    return 1;
}
```

**这段是给你看"应该长什么样"的参考实现，但请你自己写一遍。**
特别是：`uart_rx_reset()` 保留 `err_count`、清 `count`/`buffer`/`state` —— 复用它而不是手写清零逻辑，
这样"复位"的语义只有一个定义点。

---

## 3. 在 STM32 上怎么落地（面试会问）

```c
/* SysTick 每 1 ms 递增 tick */
volatile uint32_t g_tick = 0;
void SysTick_Handler(void) { g_tick++; }

/* UART 接收中断：只喂字节，这是中断里唯一要做的 */
void USART1_IRQHandler(void)
{
    if (readable)
    {
        uart_rx_feed_byte(&rx, read_byte(), g_tick);   /* 只喂，不解析 */
    }
}

/* 主循环：解析 + 超时检查 */
while (1)
{
    /* 1. 超时检查：每轮都查，阈值由 RX_TIMEOUT_MS 保证 */
    if (uart_rx_check_timeout(&rx, g_tick))
    {
        /* 超时了：记一笔，可能还要重连 */
    }

    /* 2. 取已成帧的帧来解析 */
    if (frame_ready) { command_parse(...); app_handle(...); }

    /* 3. 干别的事 */
}
```

**关键点：`g_tick` 必须是 `volatile`。**
没有 `volatile`，编译器会认为主循环里 `g_tick` 的值没变过而优化掉读取 —— 这是嵌入式里最经典的坑之一。

**另一个关键点：中断里只喂字节，不解析、不打印。**
解析和打印都放主循环。中断服务函数应该尽可能短。

---

## 4. 编译命令

```bash
gcc -std=c11 -Wall -Wextra -Werror -g -fexec-charset=UTF-8 \
    uart_rx_timeout_test.c uart_rx.c command.c app.c -o uart_rx_timeout_test.exe
```

**四个 `.c` 一个都不能少**，和第 35 课一样。

> 建议：写完后额外做一次转换体检（不进构建配置，只手动查一次）：
> ```bash
> gcc -std=c11 -Wall -Wextra -Wconversion -Werror -fsyntax-only uart_rx.c uart_rx_timeout_test.c
> ```
> 参考实现在这个标准下**零警告通过**。

---

## 5. 验收用例（期望输出已在本机实跑验证）

新建 `uart_rx_timeout_test.c`，每个用例独立跑。
**下方的输出是参考实现的真实输出，你的实现必须逐字一致。**

### 用例 1：正常帧，字节间隔都在阈值内 → 不该触发超时

```c
/* 喂 AA 04 01 FF，每字节间隔 10ms，之后查一次超时 */
```

```text
===== case 1: normal frame, no timeout =====
Frame complete
cmd=01 data_len=0
  -> play
timeout fired=0
state=0 count=0 err=0 timeout=0
```

### 用例 2：半帧断流 → 超时复位（**本课核心**）

```c
/* 喂 AA 06 A2 后停住；先查 +99ms（不该触发），再查 +101ms（该触发） */
```

```text
===== case 2: half frame stall -> timeout reset =====
after 3 bytes (mid-frame):
state=3 count=3 err=0 timeout=0
at +99ms  timeout fired=0
state=3 count=3 err=0 timeout=0
at +101ms timeout fired=1
state=0 count=0 err=0 timeout=1
```

**看两处**：
- `+99ms` 不触发、`+101ms` 触发 → 阈值是**严格大于** 100ms，不是 `>=`。
- 触发后 `state=0`（回到 `RX_WAIT_HEADER`）、`count=0`（半帧被丢弃）、`timeout=1`。

### 用例 3：超时后必须还能收好帧（**不能被超时搞死**）

```c
/* 先制造一次超时，然后紧接着喂一帧完整的 AA 04 01 FF */
```

```text
===== case 3: recover a good frame after a timeout =====
timeout fired=1
state=0 count=0 err=0 timeout=1
Frame complete
cmd=01 data_len=0
  -> play
state=0 count=0 err=0 timeout=1
```

**关键**：超时后 `timeout=1` **保留**（复位不清统计），但**新帧照样收得到**。

### 用例 4：空闲链路不算超时

```c
/* 什么都不喂，直接查一个很远的时间 */
```

```text
===== case 4: idle link is not a timeout =====
timeout fired=0
state=0 count=0 err=0 timeout=0
```

### 用例 5：两种错误计数分开

```c
/* 先制造一次长度字段非法（0x03），再制造一次半帧断流 */
```

```text
===== case 5: timeout count is separate from error count =====
after bad length:
state=0 count=0 err=1 timeout=0
timeout fired=1
state=0 count=0 err=1 timeout=1
```

**`err=1 timeout=0` → `err=1 timeout=1`**：两类计数互不干扰。

### 用例 6：tick 溢出回绕（**验证无符号减法**）

```c
/* tick 从 0xFFFFFF00 起，先查 +11ms（不该触发），再查 +311ms（该触发） */
```

```text
===== case 6: tick wraparound still detects timeout =====
after wrap, +11ms  timeout fired=0 (expect 0)
state=2 count=2 err=0 timeout=0
after wrap, +311ms timeout fired=1 (expect 1)
state=0 count=0 err=0 timeout=1
```

**这条最容易被写错**：用 `now > last + 100` 的话，第一次查询就会误判。

### 用例 7：成帧后未 reset，不能误判超时（**决策 2 的专属用例**）

```c
/* 喂完整帧 AA 04 01 FF 成帧（不要调 reset），然后查 500ms 后 */
```

```text
frame done=1 count=4 state=0
timeout after frame w/o reset fired=0 (count!=0 但已完整)
state=0 count=4
```

**两条都要看**：
- `fired=0` —— 不能把"刚收完一帧"当成断流。
- `count=4` —— 超时**没有**清掉 buffer（如果你的实现触发并 reset 了，这里会是 0）。

### 验收自查

```bash
# 超时阈值只在 protocol.h 定义
grep -c "RX_TIMEOUT_MS" protocol.h        # 应为 1

# uart_rx.c 里不应该出现裸数字超时值
grep -n "100" uart_rx.c                    # 应无输出

# 严格编译 + 转换体检
gcc -std=c11 -Wall -Wextra -Werror -g -fexec-charset=UTF-8 \
    uart_rx_timeout_test.c uart_rx.c command.c app.c -o uart_rx_timeout_test.exe
gcc -std=c11 -Wall -Wextra -Wconversion -Werror -fsyntax-only uart_rx.c uart_rx_timeout_test.c
```

---

## 6. 本课要能回答的问题（面试常问）

1. 为什么半帧断流是必现的 bug，而不是"极端情况"？
2. 超时检查为什么放在接收层**外面**？放里面有什么问题？
3. 为什么判断"半帧在途"要用 `state` 而不是 `count`？（**这题答对了，说明你真跑过边界用例**）
4. `(uint32_t)(now - last)` 为什么不怕 tick 溢出？它的前提条件是什么？
5. `timeout_count` 为什么要和 `err_count` 分开？分开之后运维上能回答什么问题？
6. `RX_TIMEOUT_MS = 100` 是怎么定出来的？改成 10 ms 或 10 s 各会怎样？
7. `g_tick` 为什么必须加 `volatile`？不加会发生什么？
8. 为什么中断服务函数里只喂字节、不解析？

第 4 题的前提条件是"两次检查的真实间隔 < 2^32 毫秒（约 49.7 天）"——
**说出这个前提，比只说"无符号减法不会溢出"更能体现你想清楚了**。

---

## 7. 本课遗留与技术债

| 项 | 状态 | 说明 |
|---|---|---|
| `uart_rx_test.c` | **不动** | 第 35 课的测试，签名改了会编译不过 —— 所以本课用新文件 `uart_rx_timeout_test.c` |
| `main.c` / `command_test.c` / `struct_test.c` | 不动 | 都没调用 `feed_byte`，不受签名变更影响 |
| `command.h` 的 `check_command` | 保留，待淘汰 | 同第 35 课 |
| `protocol.h` 的 `CMD_VOL_ARG_LEN` / `CMD_TOUCH_ARG_LEN` | 已启用 | 第 35D 之后由应用层使用 |

**下一课（第 37 课）**：把三层真正接到 STM32 的 UART 中断上 ——
`USART1_IRQHandler` + `SysTick` + `g_tick`，用真硬件跑通"上位机发一帧 → 设备播一首歌"的完整链路。

**在那之前，本课的实现必须全部通过上面 7 条验收用例。**
