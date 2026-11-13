#include "uart_rx.h"


void uart_rx_reset(uart_rx_t *ctx)
{
    ctx->count = 0;
    ctx->state = RX_WAIT_HEADER;
    for (size_t i = 0; i < RX_BUF_SIZE; i++)
    {
        ctx->buffer[i] = 0;
    }
}

void uart_rx_init(uart_rx_t *ctx)
{
    uart_rx_reset(ctx); /* 先清接收状态 */
    ctx->err_count = 0; /* 统计只在真正初始化时清，每帧复位不清 */
}

int uart_rx_feed_byte(uart_rx_t *ctx, uint8_t byte)
{
    int retry = 1; /* 帧尾判定失败时，同一个字节要回帧头状态重判一次 */

    while (retry)
    {
        retry = 0; /* 默认这一趟用完就结束 */

        switch (ctx->state)
        {
        case RX_WAIT_HEADER:
            /* 是 RX_HEADER：存 buffer[0]、count=1、state 切到 RX_WAIT_LEN
               不是：什么都不做 */
            if (byte == RX_HEADER)
            {
                ctx->buffer[0] = byte;
                ctx->count = 1;
                ctx->state = RX_WAIT_LEN;
            }

            break;

        case RX_WAIT_LEN:
            /* 这个字节就是整帧长度，先校验 RX_MIN_LEN <= byte <= RX_BUF_SIZE
               合法：len = byte、存 buffer[1]、count=2、state 切到 RX_WAIT_CMD
               非法：count=0、err_count++、state 回 RX_WAIT_HEADER */
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
                retry = 1;
            }

            break;

        case RX_WAIT_CMD:
            ctx->buffer[2] = byte;
            ctx->count = 3;
            if (ctx->len == RX_MIN_LEN)
            {
                ctx->state = RX_WAIT_TAIL;
            }
            else
            {
                ctx->state = RX_WAIT_DATA;
            }
            break;

        case RX_WAIT_DATA:
            ctx->buffer[ctx->count] = byte;
            ctx->count++;
            if (ctx->count == ctx->len - 1)
            {
                ctx->state = RX_WAIT_TAIL;
            }
            break;

        case RX_WAIT_TAIL:
            ctx->buffer[ctx->len - 1] = byte;
            ctx->count = ctx->len; /* 直接给帧长，不依赖"进来时正好是 len-1" */
            ctx->state = RX_WAIT_HEADER;
            if (byte == RX_TAIL)
            {
                return 1; /* 收到完整帧，count 保持为帧长，由调用方读取后复位 */
            }
            else
            {
                ctx->err_count++;
                ctx->count = 0;
                retry = 1; /* 这个字节没当上帧尾，退回帧头状态重判一次 */
            }
            break;
        }
    }

    return 0;
}
