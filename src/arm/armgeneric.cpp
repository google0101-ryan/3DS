#include "armgeneric.h"

#include <string>
#include <algorithm>
#include <cassert>

template<typename T>
T sign_extend(T x, int bits)
{
    T m = 1;
    m <<= bits - 1;
    return (x ^ m) - m;
}

std::string HexToString(uint32_t num)
{
    std::string ret;
    while (num)
    {
        ret += "0123456789ABCDEF"[num % 16];
        num /= 16;
    }

    std::reverse(ret.begin(), ret.end());
    return ret;
}

bool CalcOverflowSub(uint32_t a, uint32_t b, uint32_t result)
{
    bool sA = (a >> 31) & 1;
    bool sB = (b >> 31) & 1;
    bool sR = (result >> 31) & 1;
    return sA != sB && sB == sR;
}

bool CalcOverflowAdd(uint32_t a, uint32_t b, uint32_t result)
{
    bool sA = (a >> 31) & 1;
    bool sB = (b >> 31) & 1;
    bool sR = (result >> 31) & 1;
    return sA == sB && sB != sR;
}

void ARMCore::FillPipeline()
{
    pipeline[0] = Read32(*(registers[15]));
    *(registers[15]) += 4;
    pipeline[1] = Read32(*(registers[15]));
    *(registers[15]) += 4;
}

uint32_t ARMCore::AdvancePipeline()
{
    uint32_t data = pipeline[0];
    pipeline[0] = pipeline[1];
    pipeline[1] = Read32(*(registers[15]));
    return data;
}

void ARMCore::SwitchMode(uint8_t mode)
{
    switch (mode)
    {
    case MODE_SUPERVISOR:
        for (int i = 0; i < 16; i++)
            registers[i] = &regs[i];
        registers[13] = &regs_svc[0];
        registers[14] = &regs_svc[1];
        cur_spsr = &spsr_svc;
        break;
    default:
        printf("ERROR: Switch to unknown mode 0x%x\n", mode);
        exit(1);
    }
}

bool CondPassed(CPSR& cpsr, uint8_t cond)
{
    switch (cond)
    {
    case 0b0000:
        return cpsr.z;
    case 0b0001:
        return !cpsr.z;
    case 0b1110:
        return true;
    default:
        printf("Unknown condition code 0x%x\n", cond);
        exit(1);
    }
}

bool IsBranch(uint32_t instr)
{
    return ((instr >> 24) & 0xF) == 0xA;
}

bool IsSingleDataTransfer(uint32_t instr)
{
    return ((instr >> 26) & 0x3) == 1;
}

bool IsDataProcessing(uint32_t instr)
{
    return ((instr >> 26) & 0x3) == 0;
}

bool IsMRC(uint32_t instr)
{
    return ((instr >> 24) & 0xF) == 0xE
            && ((instr >> 20) & 1) == 1
            && ((instr >> 4) & 1) == 1;
}

void ARMGeneric::Branch(ARMCore *core, uint32_t instr)
{
    int32_t imm = sign_extend<int32_t>((instr & 0xFFFFFF) << 2, 26);

    *(core->registers[15]) += imm;
    core->didBranch = true;

    if (core->CanDisassemble)
        printf("b 0x%08x\n", *(core->registers[15]));
}

void ARMGeneric::SingleDataTransfer(ARMCore *core, uint32_t instr)
{
    bool i = (instr >> 25) & 1;
    bool p = (instr >> 24) & 1;
    bool u = (instr >> 23) & 1;
    bool b = (instr >> 22) & 1;
    bool w = (instr >> 21) & 1;
    bool l = (instr >> 20) & 1;

    uint8_t rn = (instr >> 16) & 0xF;
    uint8_t rd = (instr >> 12) & 0xF;

    uint32_t op2;
    std::string op2_disasm;

    if (i)
    {
        uint8_t is = (instr >> 7) & 0x1F;
        uint8_t type = (instr >> 5) & 0x3;
        uint8_t rm = instr & 0xF;

        switch (type)
        {
        default:
            printf("Unknown shift type %d\n", type);
            exit(1);
        }
    }
    else
    {
        op2 = instr & 0xFFF;
        op2_disasm = "#" + std::to_string(op2);
    }

    uint32_t addr = *(core->registers[rn]);

    if (p)
        addr += u ? op2 : -op2;

    if (l)
    {
        if (!b)
        {
            *(core->registers[rd]) = core->Read32(addr & ~3);
            if (addr & 3)
                *(core->registers[rd]) = std::rotr(*(core->registers[rd]), (addr & 3) * 8);
        }
        else
        {
            printf("TODO: ldrb\n");
            exit(1);
        }
    }
    else
    {
        printf("TODO: Store\n");
        exit(1);
    }

    if (!p)
        addr += u ? op2 : -op2;
    
    if (!p || w)
        *(core->registers[rn]) = addr;
    
    if (core->CanDisassemble)
        printf("%s%s r%d, [r%d, %s]\n", l ? "ldr" : "str", b ? "b" : "", rd, rn, op2_disasm.c_str());
}

void ARMGeneric::DataProcessing(ARMCore *core, uint32_t instr)
{
    bool i = (instr >> 25) & 1;
    bool s = (instr >> 20) & 1;
    uint8_t opcode = (instr >> 21) & 0xF;
    uint8_t rn = (instr >> 16) & 0xF;
    uint8_t rd = (instr >> 12) & 0xF;

    uint32_t operand2;
    std::string op2_disasm;

    if (i)
    {
        uint32_t imm = instr & 0xff;
        uint8_t shamt = (instr >> 8) & 0xF;

        operand2 = std::rotr(imm, shamt);

        op2_disasm = "#" + std::to_string(imm);
        if (shamt)
            op2_disasm += ", #" + std::to_string(shamt);
    }
    else
    {
        bool r = (instr >> 4) & 1;

        if (r)
        {
            printf("TODO: Shift-by-register\n");
            exit(1);
        }
        else
        {
            uint8_t rm = instr & 0xF;
            operand2 = *(core->registers[rm]);
            
            uint8_t shamt = (instr >> 7) & 0x1F;
            uint8_t shtype = (instr >> 5) & 0x3;

            op2_disasm = "r" + std::to_string(rm);
            if (shamt)
            {
                printf("TODO: Non-special case shifting\n");
                exit(1);
            }
            else
            {
                switch (shtype)
                {
                case 0:
                    break;
                default:
                    printf("Unknown shift type %d with shamt=0\n", shtype);
                    exit(1);
                }
            }
        }
    }

    switch (opcode)
    {
    case 0x0a:
    {
        uint32_t result = *(core->registers[rn]) - operand2;

        core->cpsr.n = (result >> 31) & 1;
        core->cpsr.v = CalcOverflowSub(*(core->registers[rn]), operand2, result);
        core->cpsr.c = !(operand2 > *(core->registers[rn]));
        core->cpsr.z = (result == 0);

        if (core->CanDisassemble)
            printf("cmp r%d, %s\n", rn, op2_disasm.c_str());
        break;
    }
    case 0x0b:
    {
        uint32_t result = *(core->registers[rn]) + operand2;

        core->cpsr.n = (result >> 31) & 1;
        core->cpsr.v = CalcOverflowAdd(*(core->registers[rn]), operand2, result);
        core->cpsr.c = (result < *(core->registers[rn]));
        core->cpsr.z = (result == 0);

        if (core->CanDisassemble)
            printf("cmn r%d, %s\n", rn, op2_disasm.c_str());
        break;
    }
    case 0x0c:
    {
        uint32_t result = *(core->registers[rn]) | operand2;


        if (s)
        {
            core->cpsr.z = (result == 0);
            core->cpsr.n = (result >> 31) & 1;
        }

        *(core->registers[rd]) = result;

        if (core->CanDisassemble)
            printf("orr r%d,r%d,%s\n", rd, rn, op2_disasm.c_str());

        break;
    }
    default:
        printf("Unknown data processing opcode 0x%02x\n", opcode);
        exit(1);
    }

    if (rd == 15)
        core->didBranch = true;
}

void ARMGeneric::MoveFromCP(ARMCore *core, uint32_t instr)
{
    uint8_t cpopc = (instr >> 21) & 0x7;
    uint8_t cn = (instr >> 16) & 0xF;
    uint8_t rd = (instr >> 12) & 0xF;
    uint8_t pn = (instr >> 8) & 0xF;
    assert(pn == 15);
    uint8_t cp = (instr >> 5) & 0x7;
    uint8_t cm = instr & 0xF;

    *(core->registers[rd]) = core->cp15->ReadRegister(cpopc, cn, cm, cp);

    if (core->CanDisassemble)
        printf("mrc %d, %d, r%d, C%d, C%d, {%d}\n", pn, cpopc, rd, cn, cm, cp);
}

void ARMGeneric::DoARMInstruction(ARMCore *core, uint32_t instr)
{
    printf("[ARM%d]: ", core->id);

    if (!CondPassed(core->cpsr, (instr >> 28) & 0xF))
    {
        printf("Cond failed\n");
        return;
    }

    if (IsBranch(instr))
    {
        Branch(core, instr);
        return;
    }
    else if (IsSingleDataTransfer(instr))
    {
        SingleDataTransfer(core, instr);
        return;
    }
    else if (IsDataProcessing(instr))
    {
        DataProcessing(core, instr);
        return;
    }
    else if (IsMRC(instr))
    {
        MoveFromCP(core, instr);
        return;
    }

    printf("Unhandled ARM instruction 0x%08x\n", instr);
    exit(1);
}
