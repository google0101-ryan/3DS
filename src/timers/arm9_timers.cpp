#include "arm9_timers.h"

#include <memory/Bus.h>
#include <stdio.h>
#include <cassert>

union TimerCnt
{
    uint16_t data;
    struct
    {
        uint16_t prescaler : 2;
        uint16_t overflow : 1;
        uint16_t : 3;
        uint16_t irq_en : 1;
        uint16_t start : 1;
        uint16_t : 8;
    };
};

struct Timer
{
    TimerCnt cnt;
    uint16_t reload;
    uint16_t ctr;
    uint16_t count;
} timers[4];

void Timers::Tick()
{
    int prescalars[] = {1, 64, 256, 1024};

    for (int i = 0; i < 4; i++)
    {
        assert(!timers[i].cnt.overflow);
        if (timers[i].cnt.start)
        {
            timers[i].ctr++;
            if (timers[i].ctr == prescalars[timers[i].cnt.prescaler])
            {
                timers[i].count--;
                if (timers[i].count <= 0)
                {
                    timers[i].count = timers[i].reload;
                    Bus::SetInterruptPending9(8 + i);
                }
            }
        }
    }
}

void Timers::Write16(uint32_t addr, uint16_t data)
{
    switch (addr)
    {
    case 0x10003000:
        printf("Writing 0x%04x to timer0 cnt\n", data);
        timers[0].cnt.data = data;
        break;
    case 0x10003002:
        timers[0].reload = data;
        break;
    case 0x10003004:
        printf("Writing 0x%04x to timer1 cnt\n", data);
        timers[1].cnt.data = data;
        break;
    case 0x10003006:
        timers[1].reload = data;
        break;
    case 0x10003008:
        printf("Writing 0x%04x to timer2 cnt\n", data);
        timers[2].cnt.data = data;
        break;
    case 0x1000300A:
        timers[2].reload = data;
        break;
    case 0x1000300C:
        printf("Writing 0x%04x to timer3 cnt\n", data);
        timers[3].cnt.data = data;
        break;
    case 0x1000300e:
        timers[3].reload = data;
        break;
    }
}

uint16_t Timers::Read16(uint32_t addr)
{
    switch (addr)
    {
    case 0x10003000:
        printf("[TMRS9]: Read from timer0 control\n");
        return timers[0].cnt.data;
    case 0x10003002:
        printf("[TMRS9]: Read from timer0 count\n");
        return timers[0].reload;
    case 0x10003004:
        printf("[TMRS9]: Read from timer1 control\n");
        return timers[1].cnt.data;
    case 0x10003006:
        printf("[TMRS9]: Read from timer1 count\n");
        return timers[1].reload;
    case 0x10003008:
        printf("[TMRS9]: Read from timer2 control\n");
        return timers[2].cnt.data;
    case 0x1000300A:
        printf("[TMRS9]: Read from timer2 count\n");
        return timers[2].reload;
    case 0x1000300C:
        printf("[TMRS9]: Read from timer3 control\n");
        return timers[3].cnt.data;
    case 0x1000300E:
        printf("[TMRS9]: Read from timer3 count\n");
        return timers[3].reload;
    }
}
