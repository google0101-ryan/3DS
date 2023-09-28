#include "arm9_timers.h"

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
} timers[4];

void Timers::Write16(uint32_t addr, uint16_t data)
{
    switch (addr)
    {
    case 0x10003000:
        timers[0].cnt.data = data;
        break;
    case 0x10003002:
        timers[0].reload = data;
        break;
    case 0x10003004:
        timers[1].cnt.data = data;
        break;
    case 0x10003006:
        timers[1].reload = data;
        break;
    case 0x10003008:
        timers[2].cnt.data = data;
        break;
    case 0x1000300A:
        timers[2].reload = data;
        break;
    case 0x1000300C:
        timers[3].cnt.data = data;
        break;
    case 0x1000300e:
        timers[3].reload = data;
        break;
    }
}