#include "cp15.h"

#include <stdio.h>
#include <stdlib.h>

void CP15::WriteRegister(int cpopc, int cn, int cm, int cp)
{
    uint32_t reg = (cpopc << 12) | (cn << 8) | (cm << 4) | cp;

    switch (reg)
    {
    default:
        printf("[CP15]: Write to unknown register %d,C%d,C%d,%d (0x%04x)\n", cpopc, cn, cm, cp, reg);
        exit(1);
    }
}

uint32_t CP15::ReadRegister(int cpopc, int cn, int cm, int cp)
{
    uint32_t reg = (cpopc << 12) | (cn << 8) | (cm << 4) | cp;

    switch (reg)
    {
    case 0x0fc0:
        return 0;
    default:
        printf("[CP15]: Read from unknown register %d,C%d,C%d,%d (0x%04x)\n", cpopc, cn, cm, cp, reg);
        exit(1);
    }
}
