#include <stdio.h>
#include <stdint.h>
#include "command.h"



int main(void)
{
uint8_t received[3] = {0xAA, 0x09, 0x55};
uint8_t buffer[3] = {0};
size_t count = 0;
for (size_t i = 0; i < sizeof(received) / sizeof(received[0]); i++)
{
    if (count < sizeof(buffer) / sizeof(buffer[0]))          // ① 容量上限，用 sizeof 表达，不要写 3
    {
        buffer[count] = received[i];   // ② 该写哪一格？
        count++;                        // ③ 数量怎么变？
        printf("Received: %zu bytes\n", count);  // ④ 打印谁？
        if(count==3){
            printf("Frame complete\n");
            int result = check_command(buffer, count);
            printf("Command: %d\n", result);
        }
    }
}

}
