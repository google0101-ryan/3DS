#include "mpcore_pmr.h"
#include "arm11.h"

#include <cassert>
#include <string.h>

int core_count = 2;
extern ARM11Core cores[4];

MPCore_PMR::TimerCtrl MPCore_PMR::timer0_ctrl;
MPCore_PMR::TimerCtrl MPCore_PMR::timer1_ctrl;
bool MPCore_PMR::timer0_intstatus = false, MPCore_PMR::timer1_intstatus = false;
bool MPCore_PMR::global_intrs_enabled = false;

uint32_t int_target_regs[96];
uint8_t global_int_priority[96];
uint32_t global_int_mask[8];

uint32_t timer0_reload_value, timer1_reload_value;

uint8_t MPCore_PMR::get_int_priority(int int_id)
{
    if (int_id < 32)
        return local_int_priority[int_id];
    return global_int_priority[int_id - 32];
}

uint32_t MPCore_PMR::FindHighestPending()
{
    uint32_t pending = 0x3FF;
    int last_priority = 0xF;
    for (int i = 127; i >= 0; i--)
    {
        int index = i / 32;
        int bit = i % 32;

        if (local_int_pending[index] & (1 << bit))
        {
            if (global_int_mask[index] & (1 << bit))
            {
                uint8_t priority = get_int_priority(i);
                if (priority <= last_priority)
                {
                    pending = i;
                    last_priority = priority;

                    if (i < 16)
                        pending |= private_int_requestor[i] << 10;
                }
            }
        }
    }
    return pending;
}

void MPCore_PMR::SetPendingIrq(int id, int id_of_requestor)
{
    int index = id / 32;
    int bit = id % 32;
    local_int_pending[index] |= 1 << bit;

    if (id < 16)
        private_int_requestor[id] = id_of_requestor;
}

void MPCore_PMR::Initialize()
{
    memset(int_target_regs, 0, sizeof(int_target_regs));
    memset(int_enabled, 0, sizeof(int_enabled));
    irq_cause = 0x3FF;

    for (int i = 0; i < 0x10; i++)
        int_enabled[i] = 1;
}

bool MPCore_PMR::InterruptPending()
{
    highest_priority_pending = FindHighestPending();

    int int_id = highest_priority_pending & 0x3FF;

    if (int_id == 0x3FF || !int_enabled || !global_intrs_enabled)
        return false;
    
    uint8_t priority = get_int_priority(int_id);
    if (priority < priority_mask)
    {
        uint32_t cur_active_irq = irq_cause;
        if (cur_active_irq != 0x3FF)
        {
            uint8_t active_priority = get_int_priority(cur_active_irq);
            switch (preemption_mask)
            {
            case 0x4:
                priority &= ~1;
                active_priority &= ~1;
                break;
             case 0x5: //Bits 3-2 only.
                priority &= ~0x3;
                active_priority &= ~0x3;
                break;
            case 0x6: //Bit 3 only.
                priority &= ~0x7;
                active_priority &= ~0x7;
                break;
            case 0x7: //No preemption allowed.
                return false;
            default: //All bits used
                break;
            }

            if (priority < active_priority)
            {
                printf("PREEMPTION!\n");
                exit(1);
            }
            else
                return false;
        }

        irq_cause = highest_priority_pending;
        printf("[ARM11_%d]: Servicing interrupt 0x%03x\n", coreId + 1, highest_priority_pending);
        return true;
    }

    return false;
}

void MPCore_PMR::Write8(uint32_t addr, uint8_t data)
{
    switch (addr)
    {
    case 0x17E01400 ... 0x17E0147F:
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
        int_target_regs[index] = data & 0xF;
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
        preemption_mask = data & 0x7;
        if (preemption_mask < 3)
            preemption_mask = 3;
        break;
    case 0x17E00110:
        irq_cause = 0x3FF;
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
    case 0x17e01100 ... 0x17E0111F:
        global_int_mask[(addr / 4) & 0x7] |= data;
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
    {
        bool is_aliased = ((addr / 4) & 0x7) == 0;

        if (is_aliased)
            return local_int_pending[0];
        
        uint32_t global_pending = 0;

        for (int i = 0; i < 4; i++)
            global_pending |= cores[i].pmr->local_int_pending[(addr / 4) & 0x7];
        return global_pending;
    }
    case 0x17E01280 ... 0x17E0129F:
        return 0;
    case 0x17E00604:
        return 0;
    default:
        printf("[MPCORE_PMR%d]: Read32 from unknown addr 0x%08x\n", coreId, addr);
        exit(1);
    }
}

void MPCore_PMR::AssertHWIrq(int id)
{
    uint8_t cpu_targets = int_target_regs[id - 32];

    for (int i = 0; i < core_count; i++)
    {
        if (cpu_targets & (1 << i))
            cores[i].pmr->SetPendingIrq(id);
    }
}
