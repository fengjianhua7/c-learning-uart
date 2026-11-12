# LESSON 32 · 连续多帧接收 + 帧间复位

> 学习目录：`C:\Users\fengjianhua\Desktop\learning\lesson_01`
> 本次目标（一句话）：让接收循环能连续吃下"一帧接一帧"的字节流，而不是一帧之后就把剩下的字节悄悄扔掉。
> 这是真实 UART 中断服务函数每天都在处理的情况，也是后面做环形缓冲区、半包/错帧的前提。

---

## 0. 文件安排

继续用第 31 课的 `uart_rx_test.c`，**不要动 `main.c`**。

- 第 31 课你交的是「一帧收完就解析」；
- 第 32 课在它上面加一层：**帧与帧之间的边界管理**。

`.vscode/tasks.json` 保持不变（它构建的是 `main.c + command.c`），
你练习文件自己编译：

```bash
cd /c/Users/fengjianhua/Desktop/learning/lesson_01
"/c/Users/fengjianhua/Documents/c/MinGW/bin/gcc.exe" -std=c11 -Wall -Wextra -Werror -g \
    uart_rx_test.c command.c -o uart_rx_test.exe
```

---

## 1. 上一课遗留的那个"静默丢弃"（今天的主角）

第 31B 课你写的循环长这样：

```c
for (size_t i = 0; i < sizeof(received) / sizeof(received[0]); i++)
{
    if (count < sizeof(buffer) / sizeof(buffer[0]))
    {
        buffer[count] = received[i];
        count++;
        printf("Received: %zu bytes\n", count);
        if (count == 3)
        {
            printf("Frame complete\n");
            int result = check_command(buffer, count);
            printf("Command: %d\n", result);
        }
    }
}
```

问题藏在这个 `if` 里：

- 容量检查 `count < 3` 只保证**不越界**；
- 但当 `count` 已经是 3 时，第 4 个字节进来，`if` 直接不成立 —— **这个字节被丢掉，而且没有任何提示**。

真实串口里丢一个字节，后面整条数据流都会**错位**（misalignment）：
你以为在收命令 A，其实帧头已经偏了一格，后面全是垃圾。

第 31 课你只喂了 3 个字节，所以这个 bug **没暴露**。今天开始它会暴露。

---

## 2. 任务拆成两步

### 第 32A 步：先把问题跑出来

**先不改逻辑**，只把测试数据加长，看 bug 现形。

把 `received` 改成 6 字节：

```c
uint8_t received[6] = {0xAA, 0x01, 0x55, 0xAA, 0x02, 0x55};
```

然后跑一遍，把实际输出贴出来。

你会看到第 4 个字节进来时，**"Received: 4 bytes" 根本没打印** —— 因为 `count` 停在 3，
`if` 不成立，字节被静默吞掉。

**先确认你亲眼看到了这个现象**，再往下做。

### 第 32B 步：加帧间复位

改到能正确输出两帧。

---

## 3. 需要你自己判断的设计点（这三点是本课的重点）

导师不给答案。下面每个问题都先自己想 30 秒，写下你的判断和理由，再写代码。

### 判断点 ① 复位语句放在哪一行？

候选位置：

- A：紧跟 `check_command()` 之后、`printf("Command: ...")` 之前
- B：`printf("Command: ...")` 之后、循环体末尾
- C：`if (count == 3)` 的 `else` 分支里

**想清楚三件事**：

1. 如果放在 A，`printf("Command: %d\n", result)` 打印时 `count` 已经是 0 了 —— 有没有影响？
2. 如果放在 B，中间那句 `printf` 用的还是旧 `count`，语义上更自然吗？
3. C 的 `else` 分支：什么情况下会进？进的时候 `count` 是多少？

**你的选择 + 一句话理由**，写进提交清单。

### 判断点 ② 只把 `count` 归零够不够？

这是本课真正的考点。假设你写：

```c
if (count == 3)
{
    printf("Frame complete\n");
    int result = check_command(buffer, count);
    printf("Command: %d\n", result);
    count = 0;          // ← 只清 count
}
```

**问题**：`buffer` 里还留着上一帧的三个字节（`{0xAA, 0x01, 0x55}`）。

下一帧开始写的时候：

- 第 1 个字节 `0xAA` 写进 `buffer[0]` —— 覆盖了旧的 `0xAA`，**没问题**；
- 第 2 个字节 `0x02` 写进 `buffer[1]` —— 覆盖了旧的 `0x01`，**没问题**；
- 第 3 个字节 `0x55` 写进 `buffer[2]` —— 覆盖了旧的 `0x55`，**没问题**。

所以对于**定长 3 字节**的帧，只清 `count` 在功能上是够的 —— **因为每个位置都会被覆盖**。

**但请你回答**：在什么情况下"只清 count"会出问题？

提示：想想如果帧长**不固定**，或者 `check_command` 读的不是 `buffer` 而是别的数组。
再想一个更实际的场景 —— 如果 `count` 归零了，但 `buffer[0]` 还没被新字节覆盖，
而此时程序发生了一次**中断打断了接收**（UART 中断来了但处理晚了），
你会不会读到"半个旧帧 + 半个新帧"的混合数据？

**这一题想清楚，说明你开始理解"状态机"了。**

### 判断点 ③ 溢出时怎么办？（本题不做，但要有想法）

假设某次 `count` 已经到 3，但**没有**触发复位（你判断点 ① 选错了位置），
第 4 个字节来了。现在的代码是**默默丢掉**。

真实项目里通常有三种策略：

| 策略 | 做法 | 适用场景 |
|---|---|---|
| 丢弃 | 忽略这个字节，继续等 | 偶尔有干扰字节，能容忍丢 |
| 覆盖 | 覆盖最老的一个字节（环形缓冲） | 不能丢，但可能拼出错误的帧 |
| 重同步 | 认为帧已经错位，**重新找帧头** | 协议有明确帧头，可以靠帧头重新对齐 |

**本题不做实现**，但请在提交清单里写一句：
"如果我的协议帧头是 `0xAA 0x55`，我会选哪种策略，为什么。"

---

## 4. 验收标准

### 4.1 第一组测试：两帧连续

输入 `{0xAA, 0x01, 0x55, 0xAA, 0x02, 0x55}`，期望输出：

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

**关键点**：第二帧的 `Received` 计数必须**从 1 重新开始**，不能接着数到 4、5、6。
如果你的输出是 `Received: 4 bytes` 开头，说明复位没做对。

### 4.2 第二组测试：帧内是无效命令

输入 `{0xAA, 0x02, 0x55, 0xAA, 0x09, 0x55}`，期望：

```text
Received: 1 bytes
Received: 2 bytes
Received: 3 bytes
Frame complete
Command: 2
Received: 1 bytes
Received: 2 bytes
Received: 3 bytes
Frame complete
Command: 0
```

第二帧 `0xAA 0x09 0x55` 的命令码是 `0x09` → `check_command` 返回 0（无效）。
**这一组专门验证复位后 `count` 真的归零了**：如果没归零，第二帧的 `buffer[0]` 会残留旧数据，输出就会错。

### 4.3 硬性要求

- 编译零警告（`-Wall -Wextra -Werror`）
- 运行结束 `exit code 0`
- **不要**用 `memset` 一把清整个 buffer（判断点 ② 想清楚为什么"只清 count"够用，动手时别偷懒全清 —— 除非你能说清为什么全清反而更好）
- 不要改 `command.c` / `command.h`
- 不要改 `main.c`

---

## 5. 提交清单

交这四样：

1. `uart_rx_test.c` 全文
2. 编译命令的实际输出（零警告）
3. 两组测试的完整运行输出（第 4.1 组 + 第 4.2 组）
4. 三个判断点的答案，每个**一句话理由**：
   - 复位语句放哪一行，为什么
   - 只清 `count` 够不够，在什么情况下不够
   - 帧头是 `0xAA 0x55` 时你会选哪种溢出策略

导师重点检查：

- 复位位置是否真的在"解析之后、下一字节之前"
- 有没有出现 `count` 越界（>3）或者永远归不了零
- 第二帧的 `Received` 计数有没有真的从 1 开始
- `check_command` 的返回值有没有被丢掉
- 打印顺序：先 `Frame complete` 再 `Command`，还是反过来（看你的复位位置决定打印的是哪个 `count`）

---

## 6. 复习：第 31 课的两个坑（别再踩）

| 坑 | 正确做法 |
|---|---|
| `<cstdint>` | C 里是 `<stdint.h>` |
| `uint16_t` 存串口字节 | 用 `uint8_t`，一个字节一个字节 |
| 容量用魔数 `3` | `sizeof(buffer) / sizeof(buffer[0])` |
| `main()` 省略返回类型 | `int main(void)` |
| 全局变量 | 放函数内 |

---

## 7. 学习记录 #32（进行中）

- 本次内容：连续多帧接收；帧间复位；静默丢弃与错位。
- 掌握等级：（自己填：帧边界管理 L? / 溢出策略 L?）
- 错误与薄弱点：（自己填）
- 下一项任务：（自己填）
