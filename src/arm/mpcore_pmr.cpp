#include "mpcore_pmr.h"

#include <cassert>
#include <string.h>

MPCore_PMR::TimerCtrl MPCore_PMR::timer0_ctrl;
MPCore_PMR::TimerCtrl MPCore_PMR::timer1_ctrl;
bool MPCore_PMR::timer0_intstatus = false, MPCore_PMR::timer1_intstatus = false;
bool MPCore_PMR::global_intrs_enabled = false;

uint8_t int_target_regs[256];
uint8_t global_int_priority[96];

uint32_t timer0_reload_value, timer1_reload_value;

void MPCore_PMR::Initialize()
{
    memset(int_target_regs, 0, sizeof(int_target_regs));
    memset(int_enabled, 0, sizeof(int_enabled));
    irq_cause = 0x3FF;

    for (int i = 0; i < 0x10; i++)
        int_enabled[i] = 1;
}

void MPCore_PMR::Write8(uint32_t addr, uint8_t data)
{
    switch (addr)
    {
    case 0x17E01400 ... 0x17E01480:
    {
        if (addr < 0x17E01420)
            local_int_priority[addr - 0x17E01400] = data >> 4;
        else
            global_int_priority[addr - 0x17E01420] = data >> 4;
        break;
    }
    case 0x17E01481 ... 0x17E014FF:
        break;
    case 0x17E01800 ... 0x17E018FF:
    {
        printf("Setting interrupt target %d to 0x%02x\n", addr - 0x17E01800, data);
        int index = addr - 0x17E01800;
        if (index > 0x20)
            int_target_regs[index] = data;
        break;
    }
    default:
        printf("[MPCORE_PMR%d]: Write8 0x%08x to unknown addr 0x%08x\n", coreId, data, addr);
        exit(1);
    }
}

void MPCore_PMR::Write32(uint32_t addr, uint32_t data)
{
    switch (addr)
    {
    case 0x17E00000:
        scu_control = data;
        break;
    case 0x17E00100:
        local_intr_enabled = data & 1;
        break;
    case 0x17E00104:
        priority_mask = data;
        break;
    case 0x17E00108:
        break;
    case 0x17E00608:
        timer0_ctrl.value = data;
        break;
    case 0x17E0060C:
        timer0_intstatus = 0;
        break;
    case 0x17E00628:
        timer1_ctrl.value = data;
        assert(!timer1_ctrl.enabled && !timer1_ctrl.watchdog_mode);
        break;
    case 0x17E0062C:
        timer1_intstatus = 0;
        break;
    case 0x17E00630: // We don't emulate the watchdog, so just ignore these
    case 0x17E00634:
        break;
    case 0x17E01000:
        global_intrs_enabled = data & 1;
        break;
    case 0x17E01C00 ... 0x17E01C3F:
        break;
    case 0x17e00110:
        break;
    case 0x17e01100 ... 0x17E0111F:
        if (addr > 0x17e0011F)
            int_enabled[addr - 0x17e01100] = data & 1;
        break;
    case 0x17e01180 ... 0x17E0119F:
    {
        int index = (addr >> 2) & 7;
        int_enabled[index] &= ~data;
        int_enabled[index] |= 0xFFFF;
        break;
    }
    case 0x17E01200 ... 0x17E0121F:
        break;
    case 0x17E01280 ... 0x17E0129F:
        break;
    case 0x17e00600:
        timer0_reload_value = data;
        break;
    default:
        printf("[MPCORE_PMR%d]: Write32 0x%08x to unknown addr 0x%08x\n", coreId, data, addr);
        exit(1);
    }
}

uint32_t MPCore_PMR::Read32(uint32_t addr)
{
    switch (addr)
    {
    case 0x17E00000:
        return scu_control;
    case 0x17E0010C:
        return irq_cause;
    case 0x17e01100 ... 0x17E0111F:
        return int_enabled[addr - 0x17E01100];
    case 0x17E01200 ... 0x17E0121F:
        return 0;
    case 0x17E01280 ... 0x17E0129F:
        return 0;
    case 0x17E00604:
        return 0;
    default:
        printf("[MPCORE_PMR%d]: Read32 from unknown addr 0x%08x\n", coreId, addr);
        exit(1);
    }
}
