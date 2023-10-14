#include "cp15.h"

#include <stdio.h>
#include <stdlib.h>

#include <memory/Bus.h>

void CP15::WriteRegister(int cpopc, int cn, int cm, int cp, uint32_t data)
{
    uint32_t reg = (cpopc << 12) | (cn << 8) | (cm << 4) | cp;

    switch (reg)
    {
    case 0x0100:
    {
        isMMUEnabled = data & 1;
        highExceptionVectors = (data >> 13) & 1;
        break;
    }
    case 0x0101:
        aux_control = data;
        break;
    case 0x0200:
    case 0x0201:
    case 0x0202:
    case 0x0300:
    case 0x3f00:
    case 0x3f10:
    case 0x3f20:
    case 0x3f30:
    case 0x3f40:
        // TODO: MMU registers
        break;
    case 0x0102:
    case 0x0750 ... 0x770:
    case 0x07a1:
    case 0x07a4:
    case 0x07e0:
    case 0x07e1:
    case 0x07e2:
    case 0x0850:
    case 0x0860:
    case 0x0fc0:
    case 0x0500 ... 0x0700:
        break;
    case 0x910:
        Bus::ARM9::RemapTCM(((data >> 12) & 0xFFFFF) << 12, 512 << ((data >> 1) & 0x3F), false);
        break;
    case 0x911:
        Bus::ARM9::RemapTCM(((data >> 12) & 0xFFFFF) << 12, 512 << ((data >> 1) & 0x3F), true);
        break;
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
    case 0x0005:
        return coreId;
    case 0x0100:
    {
        uint32_t reg = (highExceptionVectors << 13) | isMMUEnabled;
        return reg | (procID != 0x9 ? 0x78:0);
    }
    case 0x0101:
        return aux_control;
    case 0x0fc0:
        return 0;
    case 0x0fc1:
    case 0x0fc2:
    case 0x0fc3:
        return 0;
    default:
        printf("[CP15]: Read from unknown register %d,C%d,C%d,%d (0x%04x)\n", cpopc, cn, cm, cp, reg);
        exit(1);
    }
}
