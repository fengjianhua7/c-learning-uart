#include <stdio.h>
#include <stdint.h>
#include "command.h"
#include "uart_rx.h"
#include "app.h"

int main(void)
{
    // 切换用例时只改这一行
    // uint8_t received[] = {0xAA, 0xAA, 0x01, 0x55};
    // uint8_t received[] = {0xAA, 0x01, 0x55, 0xAA, 0x02, 0x55};
    // uint8_t received[] = {0xAA, 0x01, 0xAA, 0x02, 0x55, 0x55};
    // uint8_t received[] = {0x55, 0xAA, 0x01, 0x55};
    // uint8_t received[] = {0xAA, 0xAA, 0x55};
    // uint8_t received[] = {0xAA, 0xAA, 0xAA, 0x55};
    uint8_t received[] = {0xAA, 0x04, 0x01, 0xFF};
    uart_rx_t rx;
    uart_rx_init(&rx); // 一次初始化：接收状态 + 错误统计都归零
    command_t cmd;

    for (size_t i = 0; i < sizeof(received) / sizeof(received[0]); i++)
    {
        int done = uart_rx_feed_byte(&rx, received[i]);

        // printf("byte=%02X state=%d count=%zu\n", received[i], (int)rx.state, rx.count);

        if (done)
        {
            printf("Frame complete\n");
            if (command_parse(rx.buffer, rx.count, &cmd))
            {
                printf("cmd=%02X data_len=%zu\n", cmd.cmd, cmd.data_len);
                app_handle(&cmd);
            }
            else
            {
                printf("parse failed\n");
            }
            uart_rx_reset(&rx);
        }
    }

    printf("final count: %zu, errors: %u\n", rx.count, rx.err_count);

    return 0;
}
