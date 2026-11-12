#ifndef UART_RX_H
#define UART_RX_H

#include <stdint.h>
#include <stddef.h>

/* 帧格式：HEADER | CMD | TAIL
 * 约束：命令码不能等于 RX_HEADER(0xAA)，否则会被判定为假帧头（fake header）
 *       而无法被解析。详见 LESSON_33.md / LESSON_34.md。 */
#define RX_FRAME_LEN 3
#define RX_HEADER    0xAA
#define RX_TAIL      0x55

/* 一次接收会话的完整状态。
 * 用结构体而不是全局变量，是为了可测试、可多实例（UART + SPI 各一套）。 */
typedef struct
{
    uint8_t buffer[RX_FRAME_LEN]; /* 帧缓冲区，未收满时可能只填了前几格 */
    size_t  count;                /* 已收到的字节数，同时是下一个写入位置的下标 */
    uint32_t err_count;           /* 错误帧计数 */

} uart_rx_t;

/* 初始化：清空接收状态 + 错误统计归零。结构体在栈上是未初始化的，必须先调用。
 * 只用一次，放在"设备上电/模块挂载"的位置。 */
void uart_rx_init(uart_rx_t *ctx);

/* 复位：只清接收状态（count、buffer），保留 err_count。
 * 每处理完一帧调用一次，准备接收下一帧。 */
void uart_rx_reset(uart_rx_t *ctx);

/* 喂一个字节：内部完成位置校验、重同步、成帧解析。
 * 对应 STM32 中断服务函数里唯一要调的那一行。 */
int uart_rx_feed_byte(uart_rx_t *ctx, uint8_t byte);//返回 1 = 收到完整帧，0 = 未成帧

#endif /* UART_RX_H */
