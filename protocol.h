#ifndef PROTOCOL_H
#define PROTOCOL_H

/* 帧格式：AA | LEN | CMD | DATA... | FF
 *
 * 约束：
 *   - LEN 表示整帧总长（含帧头、LEN 自身、帧尾）。
 *   - 命令码不能等于 RX_HEADER(0xAA)，否则会被当成假帧头。
 *   - DATA 可以没有；多字节数值按大端（big-endian）排列。
 *
 * 本文件是接收层与解析层共享的协议定义，改协议只改这里。
 */

#define RX_HEADER 0xAA /* 帧头 */
#define RX_TAIL 0xFF   /* 帧尾 */
#define RX_MIN_LEN 4   /* 最短帧：AA LEN CMD FF */
#define RX_BUF_SIZE 16 /* 接收缓冲区容量，也是 LEN 的上限 */

/* 每帧的固定开销：帧头 + LEN + CMD + 帧尾 */
#define CMD_FRAME_OVERHEAD 4
/* 单帧最多携带的参数字节数 */
#define CMD_DATA_MAX (RX_BUF_SIZE - CMD_FRAME_OVERHEAD)

/* 命令码 —— 无参命令 */
#define CMD_START 0x01      /* 播放 */
#define CMD_STOP 0x02       /* 暂停 */
#define CMD_NEXT_MUSIC 0x03 /* 下一首 */
#define CMD_PREV_MUSIC 0x04 /* 上一首 */
#define CMD_VOL_UP 0x05     /* 音量加 */
#define CMD_VOL_DOWN 0x06   /* 音量减 */

/* 命令码 —— 带参命令 */
#define CMD_VOL 0xA1   /* 设置音量，参数 1 字节 */
#define CMD_TOUCH 0xA2 /* 触摸坐标，参数 4 字节（x:u16, y:u16） */

/* 带参命令的参数长度，用于解析层校验 */
#define CMD_VOL_ARG_LEN 1
#define CMD_TOUCH_ARG_LEN 4

#endif /* PROTOCOL_H */
