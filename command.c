#include "command.h"
#include <stddef.h>

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
