#include "arm9.h"

#include <memory/Bus.h>

#include <string.h>
#include <stdio.h>

ARM9Core::ARM9Core()
{
    cp15 = new CP15();
    cp15->procID = 9;

    id = 9;
}

void ARM9Core::Reset()
{
    CanDisassemble = false;

    memset(regs, 0, sizeof(regs));
    memset(regs_svc, 0, sizeof(regs_svc));
    memset(regs_fiq, 0, sizeof(regs_fiq));
    memset(regs_abt, 0, sizeof(regs_abt));
    memset(regs_irq, 0, sizeof(regs_irq));
    memset(regs_und, 0, sizeof(regs_und));

    regs[15] = 0xFFFF0008;

    SwitchMode(MODE_SUPERVISOR);

    cpsr.value = 0;
    cpsr.i = 1;
    cpsr.f = 1;
    cpsr.mode = MODE_SUPERVISOR;
}

void ARM9Core::Run()
{
    if (Bus::GetInterruptPending9() && !cpsr.i)
    {
        DoInterrupt();
        halted = false;
    }

    if (halted)
        return;

    if (cpsr.t)
    {   
        didBranch = false;

        uint16_t instr = Read16(*(registers[15]) - 4);

        if (CanDisassemble)
            printf("0x%04x (0x%08x) (t): ", *(registers[15]) - 4, instr);
        
        ARMGeneric::DoTHUMBInstruction(this, instr);
    }
    else
    {   
        didBranch = false;

        uint32_t instr = Read32(*(registers[15]) - 8);

        if (CanDisassemble)
            printf("0x%08x (0x%08x)", instr,  *(registers[15]) - 8);
        
        ARMGeneric::DoARMInstruction(this, instr);
    }

    if (cpsr.t)
    {
        if (didBranch)
			*(registers[15]) += 4;
        else
            *(registers[15]) += 2;
    }
    else
    {
        if (didBranch)
			*(registers[15]) += 8;
        else
            *(registers[15]) += 4;
    }
}

void ARM9Core::Dump()
{
    printf("[ARM9]:\n");
    ARMCore::Dump();
}

uint8_t ARM9Core::Read8(uint32_t addr)
{
    return Bus::ARM9::Read8(addr);
}

uint16_t ARM9Core::Read16(uint32_t addr)
{
    return Bus::ARM9::Read16(addr);
}

uint32_t ARM9Core::Read32(uint32_t addr)
{
    return Bus::ARM9::Read32(addr);
}

void ARM9Core::Write8(uint32_t addr, uint8_t data)
{
    Bus::ARM9::Write8(addr, data);
}

void ARM9Core::Write16(uint32_t addr, uint16_t data)
{
    Bus::ARM9::Write16(addr, data);
}

void ARM9Core::Write32(uint32_t addr, uint32_t data)
{
    Bus::ARM9::Write32(addr, data);
}