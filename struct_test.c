#include <stdio.h>
#include <stdint.h>
#include "command.h"

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
        printf("Test %u: %d", (unsigned)(i + 1), result);
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