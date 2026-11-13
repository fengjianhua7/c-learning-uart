#ifndef UART_RX_H
#define UART_RX_H

#include <stdint.h>
#include <stddef.h>
#include "protocol.h" /* 帧格式与命令码定义，接收层与解析层共享 */

/* 接收状态机的状态枚举。 */
typedef enum
{
    RX_WAIT_HEADER, /* 等帧头 0xAA */
    RX_WAIT_LEN,    /* 等长度字段 */
    RX_WAIT_CMD,    /* 等命令码 */
    RX_WAIT_DATA,   /* 等数据 */
    RX_WAIT_TAIL    /* 等帧尾 0xFF */
} rx_state_t;

/* 一次接收会话的完整状态。
 * 用结构体而不是全局变量，是为了可测试、可多实例（UART + SPI 各一套）。 */
typedef struct
{
    uint8_t buffer[RX_BUF_SIZE]; /* 帧缓冲区，未收满时可能只填了前几格 */
    size_t len;                  /* 期望的帧长，收到帧头后就确定了 */
    size_t count;                /* 已收到的字节数，同时是下一个写入位置的下标 */
    uint32_t err_count;          /* 错误帧计数 */
    rx_state_t state;            /* 当前状态：在等帧头 / 长度 / 命令码 / 数据 / 帧尾 */

} uart_rx_t;

/* 初始化：清空接收状态 + 错误统计归零。结构体在栈上是未初始化的，必须先调用。
 * 只用一次，放在"设备上电/模块挂载"的位置。 */
void uart_rx_init(uart_rx_t *ctx);

/* 复位：只清接收状态（count、buffer、state），保留 err_count。
 * 每处理完一帧调用一次，准备接收下一帧。 */
void uart_rx_reset(uart_rx_t *ctx);

/* 喂一个字节：内部完成位置校验、重同步、成帧解析。
 * 返回 1 = 收到完整帧（buffer 内容有效，count = len），0 = 未成帧。
 * 对应 STM32 中断服务函数里唯一要调的那一行。 */
int uart_rx_feed_byte(uart_rx_t *ctx, uint8_t byte);

#endif /* UART_RX_H */
