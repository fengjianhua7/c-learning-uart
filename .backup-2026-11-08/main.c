#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include "command.h"

struct TestCase
{
    uint8_t command[3];
    int expected;
};

struct TestCase tests[] = {
    {{0xAA, 0x01, 0x55}, 1},
    {{0xAA, 0x02, 0x55}, 2},
    {{0xAA, 0x03, 0x55}, 0},
    {{0x00, 0x01, 0x55}, 0}
};

int main(void)
{
    size_t x=0;
    for(size_t i=0; i < sizeof(tests)/sizeof(tests[0]); i++)
    {
        int result =check_command(tests[i].command, sizeof(tests[i].command) / sizeof(tests[i].command[0]));
        printf("Test %zu: Expected=%d Actual=%d ", i+1, tests[i].expected, result);
        
        if(result == tests[i].expected)
        {
            printf("PASS\n");
            x++;
        }
        else
        {
            printf("FAIL\n");
        }
   
    }
    uint8_t test[] = {0xAA, 0x01};
    if(check_command(NULL,3)==0)
    {
        printf("空指针测试 pass\n");
    }
    else
    {
        printf("空指针测试 fail\n");
    }

    if(check_command(test, sizeof(test)/sizeof(test[0]))==0)
    {
        printf("两个字节的数组测试 pass\n");
    }
    else
    {
        printf("两个字节的数组测试 fail\n");
    }
    //printf("\n\nSummary : %zu/%zu passed\n", x, sizeof(tests)/sizeof(tests[0]));
    if (x == sizeof(tests)/sizeof(tests[0]))
    {
        printf("Press Enter to exit...\n");
        getchar();
        return 0;  
    }
    else
    {
        return 1;
    }



}