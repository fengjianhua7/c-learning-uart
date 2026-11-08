#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include "command.h"

int main(void)
{
    uint8_t commands[3][3] = {
        {0xAA, 0x01, 0x55},
        {0xAA, 0x02, 0x55},
        {0xAA, 0x03, 0x55}
    };
    int result;
    int expected[3] = {1, 2, 0}; // 预期结果
    if (sizeof(commands) / sizeof(commands[0]) != sizeof(expected) / sizeof(expected[0]))
    {
        printf("ERROR: Test count mismatch\n");
        return 1;
    }
    for(size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++)
    {
        result = check_command(commands[i], 3);
        printf("Result %u: %d", (unsigned)(i + 1), result);
        if (result == expected[i])
        {
            printf(" pass\n");
        }
        else
        {
            printf(" FAIL\n");
        }
    }
    return 0;

}