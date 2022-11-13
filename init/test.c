#include "printk.h"
#include "defs.h"

// Please do not modify

void test()
{
    int cnt = 0;

    while (1)
    {
        if (cnt >= 190000000)
        {
            printk("kernel is running!\n");
            cnt = 0;
        }
        cnt++;
    }
}
