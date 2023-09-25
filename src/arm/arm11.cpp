#include "arm11.h"
#include <memory/Bus.h>

#include <string.h>

static int coreID = 0;

ARM11Core::ARM11Core()
{
    coreId = coreID++;
}

void ARM11Core::Reset()
{
    memset(regs, 0, sizeof(regs));
    memset(regs_svc, 0, sizeof(regs_svc));
    memset(regs_fiq, 0, sizeof(regs_fiq));
    memset(regs_abt, 0, sizeof(regs_abt));
    memset(regs_irq, 0, sizeof(regs_irq));
    memset(regs_und, 0, sizeof(regs_und));

    SwitchMode(MODE_SUPERVISOR);

    cpsr.value = 0;
    cpsr.mode = MODE_SUPERVISOR;

    id = 11;
    cp15 = new CP15();

    FillPipeline();
}

void ARM11Core::Run()
{
    if (cpsr.t)
    {
        printf("ERROR: Thumb mode\n");
        exit(1);
    }
    else
    {
        didBranch = false;

        printf("Core %d: ", coreId+1);

        uint32_t instr = AdvancePipeline();
        ARMGeneric::DoARMInstruction(this, instr);

        if (didBranch)
            FillPipeline();
        else
            *(registers[15]) += 4;
    }
}

void ARM11Core::Dump()
{
    printf("Core %d: [ARM11]:\n", coreId+1);
    ARMCore::Dump();
}

uint32_t ARM11Core::Read32(uint32_t addr)
{
    return Bus::ARM11::Read32(addr);
}