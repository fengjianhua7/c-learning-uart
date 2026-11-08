# lesson_01 · C 语言命令帧解析练习

用 C 练习**三字节命令帧的解析与校验**。核心是 `check_command()`：接收一个字节缓冲区，
判断它是哪一条命令，无效则返回错误码。

## 命令帧格式

```
+--------+--------+--------+
| 0xAA   | 0x01/02| 0x55   |
| 帧头   | 命令码 | 帧尾   |
+--------+--------+--------+
```

| 帧 | 命令码 | 返回值 | 含义 |
|----|--------|--------|------|
| `AA 01 55` | `0x01` | `1` | 播放命令 |
| `AA 02 55` | `0x02` | `2` | 暂停命令 |
| 其他 / 长度不足 / 空指针 | — | `0` | 无效输入 |

## 文件结构

| 文件 | 作用 |
|------|------|
| `command.h` | `check_command()` 的唯一声明（对外接口） |
| `command.c` | `check_command()` 的**唯一实现** |
| `main.c` | 主用例表（4 个用例 + 空指针 + 短数组边界测试），带 Summary 统计 |
| `struct_test.c` | 练习：把结构体数组当作用例表遍历 |
| `command_test.c` | 练习：二维数组 + 预期值数组，逐条比对 |
| `loop_test.c` | 练习：基础循环语法 |

> `check_command()` 只在 `command.c` 里实现一份，其余文件通过 `#include "command.h"` 调用。

## 编译

需要 MinGW GCC（本机路径 `Documents\c\MinGW\bin\gcc.exe`）：

```bash
gcc -Wall -Wextra -Werror -I. main.c command.c -o main.exe
gcc -Wall -Wextra -Werror -I. struct_test.c command.c -o struct_test.exe
gcc -Wall -Wextra -Werror -I. command_test.c command.c -o command_test.exe
```

VS Code 里已配好 `.vscode/tasks.json`，按 Ctrl+Shift+B 可直接构建。

## 运行

```bash
./main.exe
```

预期输出结尾：

```
Summary : 4/4 passed
```

退出码 `0` 表示全部通过，`1` 表示有用例失败。

## 版本控制

已纳入 git（提交 `64a6876` 为初始基线）。编译产物 `*.exe` / `*.bin` / `output/` 已由
`.gitignore` 排除，不入库。

## 下一步可以练什么

1. 把用例表抽成独立 `.h`（数据与逻辑分离），练习头文件组织
2. 引入 CUnit 或轻量 assert，把"打印 PASS/FAIL"变成可自动判定
3. 扩展帧格式：加长度校验位、CRC 校验
4. 接入真实硬件：ESP32 触摸探针（见 `Documents\Codex\2026-11-01\shou-li-d`）
   通过串口发 `{0xAA,0x01,0x55}` 帧 —— **这一步两端还没打通**
