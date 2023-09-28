#include "arm11.h"
#include <memory/Bus.h>

#include <string.h>

static int coreID = 0;

ARM11Core::ARM11Core()
{
    coreId = coreID++;
    
    cp15 = new CP15();
    pmr = new MPCore_PMR();
    pmr->Initialize();

    cp15->SetCoreID(coreId);
    cp15->procID = 11;

    pmr->coreId = coreId;
}

void ARM11Core::Reset()
{
    CanDisassemble = false;

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

    FillPipeline();
}

void ARM11Core::Run()
{
    if (halted)
        return;

    if (cpsr.t)
    {   
        didBranch = false;

        if (CanDisassemble)
            printf("Core %d (t): ", coreId+1);

        uint16_t instr = AdvancePipelineThumb();
        ARMGeneric::DoTHUMBInstruction(this, instr);
    }
    else
    {   
        didBranch = false;

        if (CanDisassemble)
            printf("Core %d: ", coreId+1);

        uint32_t instr = AdvancePipeline();
        ARMGeneric::DoARMInstruction(this, instr);
    }

    if (cpsr.t)
    {
        if (didBranch)
            FillPipelineThumb();
        else
            *(registers[15]) += 2;
    }
    else
    {
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

uint8_t ARM11Core::Read8(uint32_t addr)
{
    return Bus::ARM11::Read8(addr);
}

uint16_t ARM11Core::Read16(uint32_t addr)
{
    return Bus::ARM11::Read16(addr);
}

uint32_t ARM11Core::Read32(uint32_t addr)
{
    if ((addr & 0xFFF00000) == 0x17E00000)
        return pmr->Read32(addr);
    
    return Bus::ARM11::Read32(addr);
}

void ARM11Core::Write8(uint32_t addr, uint8_t data)
{
    if ((addr & 0xFFF00000) == 0x17E00000)
        return pmr->Write8(addr, data);

    Bus::ARM11::Write8(addr, data);
}

void ARM11Core::Write16(uint32_t addr, uint16_t data)
{
    Bus::ARM11::Write16(addr, data);
}

void ARM11Core::Write32(uint32_t addr, uint32_t data)
{
    if ((addr & 0xFFF00000) == 0x17E00000)
        return pmr->Write32(addr, data);

    Bus::ARM11::Write32(addr, data);
}
