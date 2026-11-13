#ifndef COMMAND_H
#define COMMAND_H

#include <stdint.h>
#include <stddef.h>
#include "protocol.h"

/* 一帧解析出来的内容。
 * 参数按原样复制进 data[]，不外借指向 rx.buffer 的指针 —— 调用方下次
 * 收字节时 buffer 会被覆盖，这里复制一份就不会跟着变。 */
typedef struct
{
    uint8_t cmd;                /* 命令码 */
    uint8_t data[CMD_DATA_MAX]; /* 参数原始字节，多字节数值按大端排列 */
    size_t data_len;            /* 参数字节数，0 表示无参命令 */
} command_t;

/* 解析一帧：成功返回 1 并填满 out，失败返回 0。
 * 只做格式校验与字段拆分，不解释参数含义（那是应用层的事）。 */
int command_parse(const uint8_t *buf, size_t len, command_t *out);

/* ---- 以下为旧的三字节协议（AA CMD 55），仅供 main.c 使用，待淘汰 ---- */
int check_command(uint8_t *buf, uint16_t len);

#endif
