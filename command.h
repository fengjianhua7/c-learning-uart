#ifndef COMMAND_H
#define COMMAND_H


#include <stdint.h>

int check_command(uint8_t *buf, uint16_t len);//命令码不能是 0xAA 

#endif
