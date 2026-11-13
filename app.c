#include "app.h"
#include <stdio.h>

/* 从两个字节读一个大端（big-endian）16 位无符号数。
 *
 * 协议规定高位字节在前：p[0] 是 high byte，p[1] 是 low byte。
 * 不能用 *(uint16_t *)p 强转 —— STM32 是小端 CPU，会读反；
 * 而且 p 的地址未必按 2 字节对齐，Cortex-M0 上未对齐访问会 HardFault。
 * 逐字节移位拼接与 CPU 端序无关，永远正确。 */
static uint16_t read_be16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

/* 命令码 → 名字，只为人读的输出服务。 */
static const char *name_of(uint8_t cmd)
{
    switch (cmd)
    {
    case CMD_START:
        return "play";
    case CMD_STOP:
        return "pause";
    case CMD_NEXT_MUSIC:
        return "next";
    case CMD_PREV_MUSIC:
        return "prev";
    case CMD_VOL_UP:
        return "volume up";
    case CMD_VOL_DOWN:
        return "volume down";
    case CMD_VOL:
        return "set volume";
    case CMD_TOUCH:
        return "touch";
    default:
        return "unknown";
    }
}

int app_handle(const command_t *cmd)
{
    if (cmd == NULL)
    {
        return 0;
    }

    switch (cmd->cmd)
    {
    /* 六个无参命令共用一个分支：它们只差一个动作名。 */
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
        if (cmd->data_len != CMD_VOL_ARG_LEN)
        {
            printf("  [reject] set volume expects %d byte, got %zu\n",
                   CMD_VOL_ARG_LEN, cmd->data_len);
            return 0;
        }
        printf("  -> set volume to %u\n", cmd->data[0]);
        return 1;

    case CMD_TOUCH:
    {
        if (cmd->data_len != CMD_TOUCH_ARG_LEN)
        {
            printf("  [reject] touch expects %d bytes, got %zu\n",
                   CMD_TOUCH_ARG_LEN, cmd->data_len);
            return 0;
        }

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
