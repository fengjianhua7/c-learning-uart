#include "command.h"

int command_parse(const uint8_t *buf, size_t len, command_t *out)
{
    if (buf == NULL || out == NULL || len < RX_MIN_LEN || len > RX_BUF_SIZE)
    {
        return 0; // 错误码，表示输入无效
    }
    if (buf[0] != RX_HEADER || buf[len - 1] != RX_TAIL || buf[1] != len)
    {
        return 0;
    }
    size_t n= len - CMD_FRAME_OVERHEAD ;
    if (  n > CMD_DATA_MAX)
    {
        return 0;
    }
    out->cmd = buf[2];
    for (size_t i = 0; i < n; i++)
    {
        out->data[i] = buf[3 + i];
    }
    out->data_len = n;
    return 1; // 成功解析一帧
}

int check_command(uint8_t *buf, uint16_t len)
{
    // 在这里检查 buf 是否为空、len 是否小于 3
    if (buf == NULL || len < 3)
    {
        return 0; // 错误码，表示输入无效
    }

    if (buf[0] == 0xAA && buf[1] == 0x01 && buf[2] == 0x55)
    {
        return 1; // 播放命令
    }

    if (buf[0] == 0xAA && buf[1] == 0x02 && buf[2] == 0x55)
    {
        return 2; // 暂停命令
    }
    return 0;
}
