#include "uart_rx.h"

void uart_rx_reset(uart_rx_t *ctx)
{
    ctx->count = 0;
    for (size_t i = 0; i < RX_FRAME_LEN; i++)
    {
        ctx->buffer[i] = 0;
    }
}

void uart_rx_init(uart_rx_t *ctx)
{
    uart_rx_reset(ctx);   /* 先清接收状态 */
    ctx->err_count = 0;   /* 统计只在真正初始化时清，每帧复位不清 */
}

int uart_rx_feed_byte(uart_rx_t *ctx, uint8_t byte) // 返回 1 = 收到完整帧，0 = 未成帧
{
    if (ctx->count >= RX_FRAME_LEN)
    {
        return 0; // 防御：正常流程走不到这里
    }

    // ① 帧尾位置收到非法字节：本帧作废
    //    独立 if，不用 else，因为清零后要让同一字节重新参与下面的帧头判定
    if (ctx->count == RX_FRAME_LEN - 1 && byte != RX_TAIL)
    {
        ctx->count = 0;
        ctx->err_count++;

    }

    if (ctx->count == 1 && byte == RX_HEADER)
    {
        // ② 命令码位置收到 0xAA：这是假帧头，不是命令码
        //    不写 buffer、不改 count，丢弃本字节，继续等真正的命令码
    }
    else if (ctx->count == 0 && byte == RX_HEADER)
    {
        // ③ 帧头位置，开新帧
        ctx->buffer[0] = byte;
        ctx->count = 1;

    }
    else if (ctx->count > 0)
    {
        // ④ 命令码 / 帧尾位置，正常写入
        ctx->buffer[ctx->count] = byte;
        ctx->count++;

    }

    if (ctx->count == RX_FRAME_LEN)
    {
        return 1; // 收到完整帧，count 保持为帧长，由调用方读取后复位
    }

    return 0; // 未成帧（半包），继续等后续字节
}