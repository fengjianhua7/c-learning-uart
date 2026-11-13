#ifndef APP_H
#define APP_H

#include "command.h"

/* 应用层入口：把一条解析好的命令变成动作。
 * 返回 1 = 命令已识别、参数合法并已执行；0 = 未识别或参数不合法。 */
int app_handle(const command_t *cmd);

#endif
