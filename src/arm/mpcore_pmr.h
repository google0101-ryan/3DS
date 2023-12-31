#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

class MPCore_PMR
{
private:
    uint32_t scu_control;

    union TimerCtrl
    {
        uint32_t value;
        struct
        {
            uint32_t enabled : 1;
            uint32_t auto_reload : 1;
            uint32_t int_enabled : 1;
            uint32_t watchdog_mode : 1;
            uint32_t rsv0 : 4;
            uint32_t prescalar : 8;
            uint32_t rsv1 : 16;
        };
    };
    static TimerCtrl timer0_ctrl, timer1_ctrl;

    static bool timer0_intstatus, timer1_intstatus;

    static bool global_intrs_enabled;
    bool local_intr_enabled;

    uint8_t local_int_priority[32];
    uint8_t int_enabled[256];
    uint32_t local_int_pending[8];
    uint32_t local_int_active[8];
    int private_int_requestor[16];

    uint16_t irq_cause;

    uint32_t priority_mask, highest_priority_pending, preemption_mask;

    uint8_t get_int_priority(int int_id);
    uint32_t FindHighestPending();

    void SetPendingIrq(int id, int id_of_requestor = 0);
public:
    int coreId;

    void Initialize();

    bool InterruptPending();

    void Write8(uint32_t addr, uint8_t data);
    void Write32(uint32_t addr, uint32_t data);

    uint32_t Read32(uint32_t addr);

    static void AssertHWIrq(int id);
};