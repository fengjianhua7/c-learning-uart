#include <stdio.h>
#include <stdint.h>


int check_command(uint8_t *buf, uint16_t len)
{
    // 在这里检查 buf 是否为空、len 是否小于 3
    if (buf == NULL || len < 3)
    {
        return 0; // 错误码，表示输入无效
    }

    if (buf[0] == 0xAA && buf[1] == 0x01 && buf[2] == 0x55)
    {
        return 1; // 命令有效
    }
       
    if (buf[0] == 0xAA && buf[1] == 0x02 && buf[2] == 0x55)
    {
        return 2; // 命令有效
    }
    return 0;
}

int main(void)
{
    
struct TestCase
{
    uint8_t command[3];
    int expected;
};

struct TestCase tests[3] = {
    {{0xAA, 0x01, 0x55}, 1},
    {{0xAA, 0x02, 0x55}, 2},
    {{0xAA, 0x03, 0x55}, 0}
};

    for(size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++)
    {
       int result = check_command(tests[i].command, sizeof(tests[i].command) / sizeof(tests[i].command[0]));
        printf("Test %zu: %d", i + 1, result);
        if (result == tests[i].expected)
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