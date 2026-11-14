# 第 35 课 · 从定长帧走到变长帧：三层协议栈

## 0. 为什么必须变长

第 33–34 课的帧格式是**定长**的：

```text
+--------+--------+--------+
| 0xAA   | 0x01/02| 0x55   |
| 帧头   | 命令码 | 帧尾   |
+--------+--------+--------+
```

固定 3 个字节，问题在于它**只能表达"有或没有"**。可你的播放器实际需要这些命令：

| 命令 | 需要携带的信息 |
|---|---|
| 播放 / 暂停 / 下一首 | 无 |
| **设置音量** | 一个字节（0–100） |
| **触摸坐标** | 四个字节（x、y 各 2 字节） |

定长帧要容纳触摸坐标，就得把所有帧都拉到 7 字节；无参命令白白多占 4 个字节。更糟的是**帧长写死在代码里**（`RX_FRAME_LEN`），改一次协议就要动接收层、解析层、测试三处。

**变长帧**把"这帧有多长"放进数据本身：

```text
+--------+--------+--------+---------+--------+
| 0xAA   | LEN    | CMD    | DATA... | 0xFF   |
| 帧头   | 总长   | 命令码 | 参数    | 帧尾   |
+--------+--------+--------+---------+--------+
```

`LEN` = 整帧总长（含帧头、LEN 自身、帧尾）。收到帧头后就知道后面要收几个字节。

**关键认识**：变长不是"把 3 改成变量"这么简单。它引入了三个新问题：

1. **组帧**要靠状态记住"我在等什么"——因为下一个字节的含义取决于当前进度。
2. **长度字段本身是不可信输入**——外部发来的 `LEN` 可能是 0 或 255，直接用它写缓冲区就是**缓冲区溢出**。
3. **参数怎么解释**——`DATA` 里的字节是无参命令的"多余"、还是带参命令的"坐标"，取决于 `CMD`。

这就是本课要建的三层：

```text
┌─────────────────────────────────────────────┐
│  应用层 app.c        switch(cmd) + 参数语义校验 │
├─────────────────────────────────────────────┤
│  解析层 command.c    帧格式自洽性校验 + 字段拆分   │
├─────────────────────────────────────────────┤
│  接收层 uart_rx.c    状态机组帧 + 帧尾重同步     │
└─────────────────────────────────────────────┘
        ↑ 共享 protocol.h（帧格式与命令码定义）
```

---

## 1. 三个关键决策

### 决策 1：`LEN` 字段的语义 —— 含不含自己？

| 方案 | 含义 | `AA 04 01 FF` 的 LEN |
|---|---|---|
| A 只算 DATA | LEN = 参数字节数 | 1 |
| **B 算整帧总长** | LEN = 全部字节数 | 4 |

**选 B（算整帧总长）**。理由：

- **自描述**：拿到 LEN 就能算出"还剩几个字节"，不用知道 CMD 的长度规则。
- **能交叉验证**：解析层可以检查 `buf[1] == len`——长度字段自己说了谎就判坏帧（下面决策 3 会用到）。
- **代价**：接收层要减去开销。`CMD_FRAME_OVERHEAD`（帧头+LEN+CMD+帧尾 = 4）这个常量就是为此存在。

### 决策 2：状态机 vs 计数

第 33 课用 `count` 反推位置。变长帧下 `count` 不够用了——`count == 3` 时，我不知道下一个字节是"参数第 1 个"还是"帧尾"，因为这取决于 `LEN`。

**必须显式记录"我在等什么"**：

```c
typedef enum {
    RX_WAIT_HEADER, /* 等帧头 0xAA */
    RX_WAIT_LEN,    /* 等长度字段 */
    RX_WAIT_CMD,    /* 等命令码 */
    RX_WAIT_DATA,   /* 等数据 */
    RX_WAIT_TAIL    /* 等帧尾 0xFF */
} rx_state_t;
```

每个 `case` 只回答三件事：**存不存、`count` 变不变、切到哪个状态**。

### 决策 3：解析层要不要重复校验？

接收层已经保证了帧是合法的。解析层**还要再校验一遍**。

**理由**：解析层可能被别处直接调用（喂文件、喂网络包、单元测试），不能把正确性寄托在"调用方一定会先走接收层"。**成本只是三次比较，收益是模块可以独立使用。**

这就是**分层防御**：接收层管"是不是一帧"，解析层管"帧格式是否自洽"，应用层管"这条命令合不合法"。三层各管一件事，互不越界。

---

## 2. 协议定义：`protocol.h`（已为你创建）

改协议只改这一处，接收层和解析层都 include 它。

```c
#define RX_HEADER 0xAA /* 帧头 */
#define RX_TAIL 0xFF   /* 帧尾 */
#define RX_MIN_LEN 4   /* 最短帧：AA LEN CMD FF */
#define RX_BUF_SIZE 16 /* 接收缓冲区容量，也是 LEN 的上限 */

/* 每帧的固定开销：帧头 + LEN + CMD + 帧尾 */
#define CMD_FRAME_OVERHEAD 4
/* 单帧最多携带的参数字节数 */
#define CMD_DATA_MAX (RX_BUF_SIZE - CMD_FRAME_OVERHEAD)

/* 命令码 —— 无参命令 */
#define CMD_START 0x01      /* 播放 */
#define CMD_STOP 0x02       /* 暂停 */
#define CMD_NEXT_MUSIC 0x03 /* 下一首 */
#define CMD_PREV_MUSIC 0x04 /* 上一首 */
#define CMD_VOL_UP 0x05     /* 音量加 */
#define CMD_VOL_DOWN 0x06   /* 音量减 */

/* 命令码 —— 带参命令 */
#define CMD_VOL 0xA1   /* 设置音量，参数 1 字节 */
#define CMD_TOUCH 0xA2 /* 触摸坐标，参数 4 字节（x:u16, y:u16） */

/* 带参命令的参数长度，用于解析层校验 */
#define CMD_VOL_ARG_LEN 1
#define CMD_TOUCH_ARG_LEN 4
```

**注意 `RX_MIN_LEN` 是 4 不是 3**：无参命令的帧是 `AA LEN CMD FF` —— 帧头、长度、命令码、帧尾，四个字节。

**`RX_BUF_SIZE` 同时是 LEN 的上限**：因为缓冲区只有 16 格，`LEN` 超过 16 的帧根本收不下。这两个数必须一致。

---

## 3. 接收层：状态机组帧

### 3.1 `init` 与 `reset` 必须拆开

```c
void uart_rx_reset(uart_rx_t *ctx)
{
    ctx->count = 0;
    ctx->state = RX_WAIT_HEADER;
    for (size_t i = 0; i < RX_BUF_SIZE; i++) {
        ctx->buffer[i] = 0;
    }
}

void uart_rx_init(uart_rx_t *ctx)
{
    uart_rx_reset(ctx);      /* 先清接收状态 */
    ctx->err_count = 0;      /* 统计只在真正初始化时清 */
}
```

**为什么不能合成一个？**

- `count`、`buffer`、`state` 属于**每帧必清**——每处理完一帧就要复位。
- `err_count` 属于**跨帧累积**——它是"设备上电到现在一共错过多少帧"的统计，每帧清掉就没意义了。

把它们放一起，结果就是每帧都把错误统计清零，排查时永远看到 `errors: 0`。

**这是本课最容易重复犯的错误。** 第 34 课你已经犯过一次同类错误（把 `reset` 里的 `state` 注释掉、改在 `main` 里手动补）。记住判据：**模块必须自己保证状态任何时刻合法，不能依赖调用方补一行来擦屁股。**

### 3.2 `LEN` 必须先校验再采信

```c
case RX_WAIT_LEN:
    if (byte >= RX_MIN_LEN && byte <= RX_BUF_SIZE)
    {
        ctx->len = byte;
        ctx->buffer[1] = byte;
        ctx->count = 2;
        ctx->state = RX_WAIT_CMD;
    }
    else
    {
        ctx->err_count++;
        ctx->count = 0;
        ctx->state = RX_WAIT_HEADER;
        retry = 1;   /* 这个字节可能是新帧的帧头 */
    }
    break;
```

**这一步是缓冲区溢出的防线。** 如果不校验 `byte <= RX_BUF_SIZE`，外部发来 `LEN = 200`，后面的 `ctx->buffer[ctx->count] = byte` 就会写到缓冲区外面——这是嵌入式里最常见的远程可触发漏洞。

**注释要写清"为什么"，不是"做什么"**：

```c
/* 长度字段是外部输入，必须先做范围校验再采信：
   RX_MIN_LEN <= byte <= RX_BUF_SIZE 才算合法，否则后面写 buffer 会越界。 */
```

### 3.3 重同步：同一个字节可能要判两次

这是本课最关键的机制。考虑这个字节流：

```text
AA 04 01 00 AA 04 01 FF
      ↑ 坏帧尾        ↑ 真帧头
```

第 4 个字节 `0x00` 应该是帧尾，但它是错的。此时有两种可能：

- 它就是个坏字节 → 丢弃，继续等下一个帧头
- 它**自己就是下一个帧的帧头** → 得拿它当帧头重新判一次

如果只丢弃，那么第 5 个字节 `AA` 会被当成帧头（正确），但 `LEN` 位上的 `04` 之前被吃掉了……实际结果是真帧错位。

**正确做法：重判（retry）**。

```c
int uart_rx_feed_byte(uart_rx_t *ctx, uint8_t byte)
{
    int retry = 1;              /* 同一个字节可能需要再判一次 */

    while (retry)
    {
        retry = 0;              /* 默认这一趟用完就结束 */

        switch (ctx->state)
        {
        /* ... 各 case ... */
        }
    }
    return 0;
}
```

**重同步的统一规则**：

> **当前位置不可能容纳这个字节，就把它当可能的帧头重判一次。**

"重判一次"而不是"循环重判"——因为一个字节最多只可能是帧头，重判一次就够了。写成 `while` 是为了能"回到帧头状态后重新走一遍 switch"，而不是无限循环。

**注意 `RX_WAIT_LEN` 的非法分支也要 `retry = 1`**。这一点很容易漏：

- 帧尾判错 → 要重判（坏尾 `AA` 可能就是下一帧的帧头）
- 长度非法 → **也要重判**（这个 `AA` 可能就是下一帧的帧头）

漏掉后者会导致一个隐蔽 bug：**坏尾后紧跟真帧头时，真帧头被当成了 LEN，导致整帧错位。** 你自己写完代码后一定要用这种"两帧连发 + 中间插坏字节"的用例验证，只测单条用例通过不算数。

### 3.4 `count` 的两种含义

```c
case RX_WAIT_DATA:
    ctx->buffer[ctx->count] = byte;
    ctx->count++;
    if (ctx->count == ctx->len - 1)   /* 下一格就是帧尾了 */
    {
        ctx->state = RX_WAIT_TAIL;
    }
    break;

case RX_WAIT_TAIL:
    ctx->buffer[ctx->len - 1] = byte;
    ctx->count = ctx->len;   /* 直接给帧长 */
    ...
```

`count` 在接收过程中是"**已收到的字节数**，也就是下一个写入位置的下标"。到了帧尾状态，直接赋值 `ctx->count = ctx->len`，**不要依赖"进来时正好是 len-1"这个隐式契约**——那是最容易在改动时崩掉的地方。

---

## 4. 解析层：拆出命令与参数

### 4.1 结构体出参

```c
typedef struct {
    uint8_t cmd;                /* 命令码 */
    uint8_t data[CMD_DATA_MAX]; /* 参数原始字节，多字节数值按大端排列 */
    size_t data_len;            /* 参数字节数，0 表示无参命令 */
} command_t;

int command_parse(const uint8_t *buf, size_t len, command_t *out);
```

**参数必须复制进 `out->data`，不能外借指向 `rx.buffer` 的指针。**

理由：`rx.buffer` 会被下一次接收覆盖。如果 `command_t` 里存的是指针，那么解析完一帧、去处理下一帧时，上一帧的参数内容**已经被改掉了**。这是嵌入式里非常隐蔽的一类 bug——因为它不报错，只是值悄悄变了。

### 4.2 三层防御 + 格式三验

```c
int command_parse(const uint8_t *buf, size_t len, command_t *out)
{
    /* 第一层：输入本身有效吗 */
    if (buf == NULL || out == NULL || len < RX_MIN_LEN || len > RX_BUF_SIZE)
        return 0;

    /* 第二层：帧格式自洽吗 —— 帧头、帧尾、长度字段三者互证 */
    if (buf[0] != RX_HEADER || buf[len - 1] != RX_TAIL || buf[1] != len)
        return 0;

    /* 第三层：参数长度合法吗 */
    size_t n = len - CMD_FRAME_OVERHEAD;
    if (n > CMD_DATA_MAX)
        return 0;

    /* 拆分字段 */
    out->cmd = buf[2];
    for (size_t i = 0; i < n; i++)
        out->data[i] = buf[3 + i];
    out->data_len = n;
    return 1;
}
```

**`buf[1] != len` 就是决策 1 里说的"交叉验证"**：LEN 自己声称总长是 `len`，如果对不上，说明这个帧被截断或损坏，直接判坏。

**这里藏着一个 C 的经典陷阱**：

```c
size_t n = len - CMD_FRAME_OVERHEAD;
```

`len` 是 `size_t`（无符号）。如果 `len < CMD_FRAME_OVERHEAD`（比如 len=3），`len - 4` 会**下溢（underflow）**成 `18446744073709551615` 这样的极大值。

本例中 `len < RX_MIN_LEN(4)` 已经被第一层挡掉了，所以安全。**但这属于"侥幸"——安全是靠别人的检查兜住的，不是自己保证的。** 写这类代码时要养成习惯：**做减法前先确认被减数不小于减数**。

### 4.3 `&&` 优先级坑

```c
/* 错 */
size_t n = len - CMD_FRAME_OVERHEAD && n > CMD_DATA_MAX;
```

`&&`（逻辑与）的优先级**高于** `=`（赋值）。这句话实际被解析成：

```c
size_t n = ((len - CMD_FRAME_OVERHEAD) && (n > CMD_DATA_MAX));
```

`n` 被赋了一个布尔值（0 或 1），而且右边的 `n` 用的是**未初始化**的值——这里会直接编译报错。

正确写法是拆成两句：

```c
size_t n = len - CMD_FRAME_OVERHEAD;
if (n > CMD_DATA_MAX) return 0;
```

**这叫"一行只干一件事"。** 它能同时消灭优先级错误和作用域错误两类 bug，成本只是多一行。

---

## 5. 应用层：分发与参数语义校验

### 5.1 大端解析：不能强转

```c
static uint16_t read_be16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}
```

协议规定高字节在前，而 **STM32 是小端 CPU**。所以：

```c
uint16_t x = *(uint16_t *)cmd->data;      /* 错！会读反 */
```

两个问题：

1. **读反**：小端机按低位在前解释字节，`2C 00` 被读成 `0x002C = 44`，而正确答案是 `0x012C = 300`。
2. **可能 HardFault**：Cortex-M0 不支持未对齐访问，`cmd->data` 的地址未必是偶数，强转成 `uint16_t*` 去读会触发硬件异常。

**逐字节移位拼接与 CPU 端序无关，永远正确。**

### 5.2 参数语义校验

格式合法 ≠ 语义合法。`AA 05 01 11 FF` 是一帧格式完全合法的帧，但 `CMD_START`（播放）不该带参数。

```c
int app_handle(const command_t *cmd)
{
    if (cmd == NULL) return 0;

    switch (cmd->cmd)
    {
    /* 六个无参命令共用一个分支：它们只差一个动作名 */
    case CMD_START:
    case CMD_STOP:
    case CMD_NEXT_MUSIC:
    case CMD_PREV_MUSIC:
    case CMD_VOL_UP:
    case CMD_VOL_DOWN:
        if (cmd->data_len != 0)
        {
            printf("  [reject] %s expects no argument, got %zu bytes\n",
                   name_of(cmd->cmd), cmd->data_len);
            return 0;
        }
        printf("  -> %s\n", name_of(cmd->cmd));
        return 1;

    case CMD_VOL:
        if (cmd->data_len != CMD_VOL_ARG_LEN) { /* ... reject ... */ }
        printf("  -> set volume to %u\n", cmd->data[0]);
        return 1;

    case CMD_TOUCH:
    {
        if (cmd->data_len != CMD_TOUCH_ARG_LEN) { /* ... reject ... */ }
        uint16_t x = read_be16(cmd->data);
        uint16_t y = read_be16(&cmd->data[2]);
        printf("  -> touch (%u, %u)\n", x, y);
        return 1;
    }

    default:
        printf("  [reject] unknown command %02X\n", cmd->cmd);
        return 0;
    }
}
```

**注意 `case CMD_TOUCH:` 后面必须加 `{ }`。** C 的 `case` 标签后不能直接跟变量声明（`uint16_t x = ...` 是声明，不是语句），会编译报错。用 `{}` 把分支包成一块就解决了。

### 5.3 三个层次的职责，用一个测试就能看清

拿 `AA 05 01 11 FF` 这帧（格式合法但语义非法）走一遍：

```text
接收层：组帧成功 → 返回 1                        ← "是不是一帧" ✓
解析层：帧头✓ 帧尾✓ LEN==5✓ → 拆分出 cmd=01, data_len=1  ← "格式自洽吗" ✓
应用层：CMD_START 但 data_len != 0 → [reject]      ← "命令合不合法" ✗
```

**分层设计的好处在测试输出里看得最清楚**：前两层都放行了，只有应用层拦下。每一层的判断依据互不重叠，出了问题能立刻定位到层。

---

## 6. 编译命令

```bash
gcc -std=c11 -Wall -Wextra -Werror -g -fexec-charset=UTF-8 \
    uart_rx_test.c uart_rx.c command.c app.c -o uart_rx_test.exe
```

**四个 `.c` 一个都不能少。** 少 `app.c` 会链接报错 `undefined reference to 'app_handle'`；少 `uart_rx.c` 同理。

> `app.c` 里的 `read_be16` 如果暂时不用，`-Werror` 会直接报 `unused-function` 失败。**严格编译不允许"以后会用到"的死代码**——要么实现 `CMD_TOUCH`，要么把函数删掉。

`.vscode/tasks.json` 里"build: uart_rx_test (练习)"这个任务已经配好四个文件，`Ctrl+Shift+B` 直接可用。

---

## 7. 验收用例（全部已在本机实跑验证）

### 用例 1：无参命令

```c
uint8_t received[] = {0xAA, 0x04, 0x01, 0xFF};
```

```text
Frame complete
cmd=01 data_len=0
  -> play
final count: 0, errors: 0
```

### 用例 2：触摸坐标（验证大端解析）

```c
uint8_t received[] = {0xAA, 0x08, 0xA2, 0x01, 0x2C, 0x00, 0x64, 0xFF};
```

```text
Frame complete
cmd=A2 data_len=4
  -> touch (300, 100)
final count: 0, errors: 0
```

**这一条是端序的照妖镜。** `01 2C` 必须是 300、`00 64` 必须是 100。如果读成 `(11264, 25600)`，就是用了 `*(uint16_t*)` 强转。

### 用例 3：设置音量

```c
uint8_t received[] = {0xAA, 0x05, 0xA1, 0x40, 0xFF};
```

```text
Frame complete
cmd=A1 data_len=1
  -> set volume to 64
final count: 0, errors: 0
```

### 用例 4：格式非法（帧尾错）

```c
uint8_t received[] = {0xAA, 0x04, 0x01, 0x00};
```

```text
final count: 0, errors: 1
```

没有 `Frame complete`——帧尾不对，接收层就不该说"成帧了"。

### 用例 5：**重同步**（坏尾后紧跟真帧）

```c
uint8_t received[] = {0xAA, 0x04, 0x01, 0x00, 0xAA, 0x04, 0x01, 0xFF};
```

```text
Frame complete
cmd=01 data_len=0
  -> play
final count: 0, errors: 1
```

**这条最关键**：坏尾之后的真帧必须被正确检出。如果重同步逻辑写错，这条会输出 `final count: 0, errors: 2`（真帧丢失）或者干脆收不到。

### 用例 6：前导垃圾字节

```c
uint8_t received[] = {0x00, 0xAA, 0x04, 0x01, 0xFF};
```

```text
Frame complete
cmd=01 data_len=0
  -> play
final count: 0, errors: 0
```

### 用例 7：长度字段非法

```c
uint8_t received[] = {0xAA, 0x03, 0x01, 0xFF};
```

```text
final count: 0, errors: 1
```

`LEN = 3` 小于最短帧长 4 → 判非法。注意这里**不会**越界写缓冲区。

### 用例 8：语义非法（参数个数不对）

```c
uint8_t received[] = {0xAA, 0x05, 0x01, 0x11, 0xFF};
```

```text
Frame complete
cmd=01 data_len=1
  [reject] play expects no argument, got 1 bytes
final count: 0, errors: 0
```

**注意 `errors: 0`**——这帧格式完全合法，只是语义不对，所以接收层和解析层都不算错误，是应用层拦下的。

### 验收自查

```bash
# 协议常量只在 protocol.h 定义
grep -c "0xAA\|0xFF" protocol.h

# uart_rx.c 里不应该出现裸数字帧头帧尾
grep -n "0xAA\|0xFF" uart_rx.c      # 应该没有输出
```

---

## 8. 本课要能回答的问题（面试常问）

1. 为什么 `LEN` 字段必须先校验范围再采信？不校验会导致什么后果？
2. 什么是重同步？为什么需要 `retry` 机制？
3. 解析层已经校验过帧格式了，为什么还要再校验一遍？
4. 为什么参数必须**复制**进 `command_t`，不能存指针？
5. 为什么 `read_be16` 不能用 `*(uint16_t *)p` 强转？（两个原因）
6. `init` 和 `reset` 为什么要分开？哪个字段不能被 `reset` 清？
7. 大端协议 + 小端 CPU，除了读反还会有什么硬件风险？
8. `size_t n = len - 4;` 有什么隐患？什么时候才会真正出问题？

第 8 题的答案：**只有当 `len < 4` 且这个检查被绕过时才会真正出问题**。本课第一层的 `len < RX_MIN_LEN` 挡住了它，所以是"侥幸安全"——**面试时主动说出这一点，比只说"我加了检查"更能体现你懂自己在依赖什么。**

---

## 9. 本课遗留的技术债

| 项 | 状态 | 说明 |
|---|---|---|
| `command.h` 的 `check_command` | 保留，待淘汰 | 旧三字节协议（`AA CMD 55`），`main.c` / `command_test.c` / `struct_test.c` 还在用。等原工程迁移完再删 |
| `uart_rx.c` 尾部旧 if 链注释 | 已清理 | 第 35E 完成 |
| `protocol.h` 的 `CMD_VOL_ARG_LEN` / `CMD_TOUCH_ARG_LEN` | 已启用 | 第 35D 之后由应用层使用 |

**下一课（第 36 课）**：接收超时（timeout）——半帧断流时如何复位、定时器如何配合。之后是把三层接到 STM32 的 UART 中断上。
