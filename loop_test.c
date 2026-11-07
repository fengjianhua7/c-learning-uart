#include <stdio.h>
#include <stdint.h>

int main(void)
{
    uint8_t volume[5] = {10, 20, 30, 40, 50};

    for(int i=0; i<5; i++)
    {
        printf("%d\n",volume[i]);
    }
   return 0;
    }
