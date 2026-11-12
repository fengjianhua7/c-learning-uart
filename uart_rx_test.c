#include <stdio.h>
#include <stdint.h>
#include "command.h"
#include "uart_rx.h"

int main(void)
{
    // 切换用例时只改这一行
    uint8_t received[] = {0xAA, 0xAA, 0x01, 0x55};
    // uint8_t received[] = {0xAA, 0x01, 0x55, 0xAA, 0x02, 0x55};
    // uint8_t received[] = {0xAA, 0x01, 0xAA, 0x02, 0x55, 0x55};
    // uint8_t received[] = {0x55, 0xAA, 0x01, 0x55};
    // uint8_t received[] = {0xAA, 0xAA, 0x55};
    // uint8_t received[] = {0xAA, 0xAA, 0xAA, 0x55};

    uart_rx_t rx;
    uart_rx_init(&rx); // 一次初始化：接收状态 + 错误统计都归零

    for (size_t i = 0; i < sizeof(received) / sizeof(received[0]); i++)
    {
        if (uart_rx_feed_byte(&rx, received[i]))
        {
            printf("Frame complete\n");
            int result = check_command(rx.buffer, rx.count);
            printf("Command: %d\n", result);
            uart_rx_reset(&rx); // 只复位接收状态，err_count 保留继续累计
        }
    }
    printf("final count: %zu, errors: %u\n", rx.count, rx.err_count);

    return 0;
}
