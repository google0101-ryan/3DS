#include "armgeneric.h"

#include <string>
#include <algorithm>
#include <cassert>
#include <bitset>

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

void ARMCore::FillPipeline()
{
    pipeline[0] = Read32(*(registers[15]));
    *(registers[15]) += 4;
    pipeline[1] = Read32(*(registers[15]));
    *(registers[15]) += 4;
}

void ARMCore::FillPipelineThumb()
{
    t_pipeline[0] = Read16(*(registers[15]));
    *(registers[15]) += 2;
    t_pipeline[1] = Read16(*(registers[15]));
    *(registers[15]) += 2;
}

uint32_t ARMCore::AdvancePipeline()
{
    uint32_t data = pipeline[0];
    pipeline[0] = pipeline[1];
    pipeline[1] = Read32(*(registers[15]));
    return data;
}

uint16_t ARMCore::AdvancePipelineThumb()
{
    uint16_t data = t_pipeline[0];
    t_pipeline[0] = t_pipeline[1];
    t_pipeline[1] = Read16(*(registers[15]));
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
    case MODE_IRQ:
        for (int i = 0; i < 16; i++)
            registers[i] = &regs[i];
        registers[13] = &regs_irq[0];
        registers[14] = &regs_irq[1];
        cur_spsr = &spsr_irq;
        break;
    case MODE_USER:
    case MODE_SYSTEM:
        for (int i = 0; i < 16; i++)
            registers[i] = &regs[i];
        cur_spsr = nullptr;
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
    case 0b0010:
        return cpsr.c;
    case 0b0011:
        return !cpsr.c;
    case 0b0100:
        return cpsr.n;
    case 0b0101:
        return !cpsr.n;
    case 0b1000:
        return cpsr.c && !cpsr.z;
    case 0b1001:
        return !cpsr.c || cpsr.z;
    case 0b1010:
        return cpsr.n == cpsr.v;
    case 0b1011:
        return cpsr.v != cpsr.n;
    case 0b1100:
        return !cpsr.z && (cpsr.v == cpsr.n);
    case 0b1101:
        return cpsr.z || (cpsr.v != cpsr.n);
    case 0b1110:
        return true;
    default:
        printf("Unknown condition code 0x%x\n", cond);
        exit(1);
    }
}

bool IsBranchExchange(uint32_t instr)
{
    return ((instr >> 4) & 0xFFFFFF) == 0x12FFF1;
}

bool IsBlockDataTransfer(uint32_t instr)
{
    return ((instr >> 25) & 0x7) == 4;
}

bool IsBranch(uint32_t instr)
{
    return ((instr >> 25) & 0x7) == 0x5;
}

bool IsSingleDataTransfer(uint32_t instr)
{
    return ((instr >> 26) & 0x3) == 1;
}

bool IsBlxReg(uint32_t instr)
{
    return ((instr >> 4) & 0xFFFFFF) == 0x12FFF3;
}

bool IsHalfWordTransferReg(uint32_t instr)
{
    return ((instr >> 25) & 0x7) == 0
        && ((instr >> 8) & 0xF) == 0
        && ((instr >> 22) & 1) == 0
        && ((instr >> 7) & 1) == 1
        && ((instr >> 4) & 1) == 1;
}

bool IsHalfWordTransferImm(uint32_t instr)
{
    return ((instr >> 25) & 0x7) == 0
        && ((instr >> 8) & 0xF) == 0
        && ((instr >> 22) & 1) == 1
        && ((instr >> 7) & 1) == 1
        && ((instr >> 4) & 1) == 1;
}

bool IsPSRTransferMSR(uint32_t instr)
{
    return ((instr >> 26) & 0x3) == 0
            && ((instr >> 23) & 0x3) == 2
            && ((instr >> 21) & 1) == 1
            && ((instr >> 12) & 0xF) == 0xF
            && ((instr >> 20) & 1) == 0;
}

bool IsPSRTransferMRS(uint32_t instr)
{
    return ((instr >> 26) & 0x3) == 0
            && ((instr >> 23) & 0x3) == 2
            && ((instr >> 21) & 1) == 0
            && ((instr >> 16) & 0xF) == 0xF
            && ((instr >> 20) & 1) == 0;
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

bool IsMCR(uint32_t instr)
{
    return ((instr >> 24) & 0xF) == 0xE
            && ((instr >> 20) & 1) == 0
            && ((instr >> 4) & 1) == 1;
}

bool IsBlxOffset(uint32_t instr)
{
    return ((instr >> 25) & 0x7) == 0x5;
}

bool IsUMULL(uint32_t instr)
{
    return ((instr >> 21) & 0x7F) == 0x4
            && ((instr >> 4) & 0xF) == 0x9;
}

bool IsMLA(uint32_t instr)
{
    return ((instr >> 21) & 0x7F) == 0x1
            && ((instr >> 4) & 0xF) == 0x9;
}

bool IsCLZ(uint32_t instr)
{
    return ((instr >> 16) & 0xFFF) == 0x16F;
}

bool IsWFI(uint32_t instr)
{
    return (instr & 0xFFFFFFF) == 0x320F003;
}

bool IsModeFlagChange(uint32_t instr)
{
    return ((instr >> 20) & 0xFFF) == 0xF10
            && ((instr >> 9) & 0xFF) == 0;
}

void ARMGeneric::BranchExchange(ARMCore *core, uint32_t instr)
{
    uint8_t rn = instr & 0xf;

    core->cpsr.t = *(core->registers[rn]) & 1;
    *(core->registers[15]) = *(core->registers[rn]) & ~1;
    core->didBranch = true;

    if (core->CanDisassemble)
        printf("bx r%d\n", rn);
}

void ARMGeneric::BlockDataTransfer(ARMCore *core, uint32_t instr)
{
    bool p = (instr >> 24) & 1;
    bool u = (instr >> 23) & 1;
    bool s = (instr >> 22) & 1;
    bool w = (instr >> 21) & 1;
    bool l = (instr >> 20) & 1;

    uint8_t rn = (instr >> 16) & 0xF;

    uint16_t reglist = instr & 0xFFFF;

    uint32_t addr = *(core->registers[rn]);

    if (core->CanDisassemble)
    {
        if (rn == 13)
        {
            switch ((l << 2) | (p << 1) | u)
            {
            case 0: printf("stmed "); break;
            case 1: printf("stmea "); break;
            case 2: printf("stmfd "); break;
            case 3: printf("stmfa "); break;
            case 4: printf("ldmfa "); break;
            case 5: printf("ldmfd "); break;
            case 6: printf("ldmea "); break;
            case 7: printf("ldmed "); break;
            }
        }
        else
        {
            switch ((l << 2) | (p << 1) | u)
            {
            case 0: printf("stmda "); break;
            case 1: printf("stmia "); break;
            case 2: printf("stmdb "); break;
            case 3: printf("stmib "); break;
            case 4: printf("ldmda "); break;
            case 5: printf("ldmia "); break;
            case 6: printf("ldmdb "); break;
            case 7: printf("ldmib "); break;
            }
        }

        printf("r%d%s, ", rn, w ? "!" : "");

        int count = std::bitset<16>(reglist).count();

        bool print_comma = true;
        int num_printed = 0;
        printf("{");
        for (int i = 0; i < 16; i++)
            if (reglist & (1 << i))
            {
                if (num_printed == (count-1))
                    print_comma = false;
                printf("r%d%s", i, print_comma ? ", " : "");
                num_printed++;
            }

        printf("} (0x%08x)\n", *(core->registers[rn]));
    }

    assert(!s);
    
    int offset;
    if (u)
        offset = 4;
    else
        offset = -4;
    
    if (l)
    {
        int regs = 0;
        if (u)
        {
            for (int i = 0; i < 16; i++)
            {
                int bit = 1 << i;
                if (reglist & bit)
                {
                    regs++;
                    if (p)
                    {
                        addr += offset;
                        *(core->registers[i]) = core->Read32(addr);
                    }
                    else
                    {
                        *(core->registers[i]) = core->Read32(addr);
                        addr += offset;
                    }
                }
            }
        }
        else
        {
            for (int i = 15; i >= 0; i--)
            {
                int bit = 1 << i;
                if (reglist & bit)
                {
                    regs++;
                    if (p)
                    {
                        addr += offset;
                        *(core->registers[i]) = core->Read32(addr);
                    }
                    else
                    {
                        *(core->registers[i]) = core->Read32(addr);
                        addr += offset;
                    }
                }
            }
        }
    }
    else
    {
        int regs = 0;
        if (u)
        {
            for (int i = 0; i < 16; i++)
            {
                int bit = 1 << i;
                if (reglist & bit)
                {
                    regs++;
                    if (p)
                    {
                        addr += offset;
                        core->Write32(addr, *(core->registers[i]));
                    }
                    else
                    {
                        core->Write32(addr, *(core->registers[i]));
                        addr += offset;
                    }
                }
            }
        }
        else
        {
            for (int i = 15; i >= 0; i--)
            {
                int bit = 1 << i;
                if (reglist & bit)
                {
                    regs++;
                    if (p)
                    {
                        addr += offset;
                        core->Write32(addr, *(core->registers[i]));
                    }
                    else
                    {
                        core->Write32(addr, *(core->registers[i]));
                        addr += offset;
                    }
                }
            }
        }
    }

    if (w && (!l || !(reglist & (1 << rn))))
        *(core->registers[rn]) = addr;

    
    if ((reglist & (1 << 15)) && l)
    {
        if (*(core->registers[15]) & 1)
        {
            core->cpsr.t = 1;
            *(core->registers[15]) &= ~1;
        }
        core->didBranch = true;
    }
}

void ARMGeneric::Branch(ARMCore *core, uint32_t instr)
{
    bool l = (instr >> 24) & 1;
    int32_t imm = sign_extend<int32_t>((instr & 0xFFFFFF) << 2, 26);

    if (l)
        *(core->registers[14]) = *(core->registers[15]) - 4;

    *(core->registers[15]) += imm;
    core->didBranch = true;

    if (core->CanDisassemble)
        printf("b%s 0x%08x\n", l ? "l" : "", *(core->registers[15]));
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

        op2 = *(core->registers[rm]);
        op2_disasm = "r" + std::to_string(rm);

        switch (type)
        {
        case 0:
            if (!is)
                break;
            core->cpsr.c = (op2 >> (is-1)) & 1;
            op2 <<= is;
            op2_disasm += ", #" + std::to_string(is);
            break;
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
            *(core->registers[rd]) = core->Read8(addr);
        }
    }
    else
    {
        if (!b)
        {
            core->Write32(addr & ~3, *(core->registers[rd]));
        }
        else
        {
            core->Write8(addr, *(core->registers[rd]));
        }
    }

    if (!p)
        addr += u ? op2 : -op2;
    
    if (!p || w)
        *(core->registers[rn]) = addr;
    
    if (rd == 15 && l)
    {
        if (*(core->registers[15]) & 1)
        {
            core->cpsr.t = *(core->registers[15]) & 1;
            *(core->registers[15]) &= ~1;
        }

        core->didBranch = true;
    }
    
    if (core->CanDisassemble)
        printf("%s%s r%d, [r%d, %s]\n", l ? "ldr" : "str", b ? "b" : "", rd, rn, op2_disasm.c_str());
}

void ARMGeneric::BlxReg(ARMCore *core, uint32_t instr)
{
    uint8_t rn = instr & 0xF;

    uint32_t target = *(core->registers[rn]);
    *(core->registers[14]) = *(core->registers[15]) - 4;
    core->cpsr.t = target & 1;
    *(core->registers[15]) = target & ~1;
    core->didBranch = true;

    if (core->CanDisassemble)
        printf("blx r%d\n", rn);
}

void ARMGeneric::HalfwordDataTransferReg(ARMCore *core, uint32_t instr)
{
    bool p = (instr >> 24) & 1;
    bool u = (instr >> 23) & 1;
    bool w = (instr >> 21) & 1;
    bool l = (instr >> 20) & 1;
    uint8_t sh = (instr >> 5) & 0x3;

    uint8_t rn = (instr >> 16) & 0xF;
    uint8_t rd = (instr >> 12) & 0xF;
    uint8_t rm = instr & 0xf;

    uint32_t addr = *(core->registers[rn]);
    
    if (p)
        addr = u ? (addr - *(core->registers[rm])) : (addr + *(core->registers[rm]));

    if  (l)
    {
        switch (sh)
        {
        default:
            printf("Unknown sh=%d, l=1\n", sh);
            exit(1);
        }
    }
    else
    {
        switch (sh)
        {
        case 1:
            core->Write16(addr & ~1, *(core->registers[rd]));
            if (core->CanDisassemble)
                printf("strh r%d, [r%d, r%d]\n", rd, rn, rm);
            break;
        default:
            printf("Unknown sh=%d, l=0\n", sh);
            exit(1);
        }
    }
    
    if (!p)
        addr = u ? (addr - *(core->registers[rm])) : (addr + *(core->registers[rm]));

    if (!p || w)
        *(core->registers[rn]) = addr;
}

void ARMGeneric::HalfwordDataTransferImm(ARMCore *core, uint32_t instr)
{
    bool p = (instr >> 24) & 1;
    bool u = (instr >> 23) & 1;
    bool w = (instr >> 21) & 1;
    bool l = (instr >> 20) & 1;
    uint8_t sh = (instr >> 5) & 0x3;

    uint8_t rn = (instr >> 16) & 0xF;
    uint8_t rd = (instr >> 12) & 0xF;
    uint32_t offset = (((instr >> 8) & 0xF) << 4) | (instr & 0xF);

    uint32_t addr = *(core->registers[rn]);
    
    if (p)
        addr = u ? (addr - offset) : (addr + offset);

    if  (l)
    {
        switch (sh)
        {
        case 1:
            *(core->registers[rd]) = core->Read16(addr & ~1);
            if (core->CanDisassemble)
                printf("ldrh r%d, [r%d, #%d]\n", rd, rn, offset);
            break;
        default:
            printf("Unknown sh=%d, l=1, i=1\n", sh);
            exit(1);
        }
    }
    else
    {
        switch (sh)
        {
        case 1:
            core->Write16(addr & ~1, *(core->registers[rd]));
            if (core->CanDisassemble)
                printf("strh r%d, [r%d, #%d]\n", rd, rn, offset);
            break;
        default:
            printf("Unknown sh=%d, l=0, i=1\n", sh);
            exit(1);
        }
    }
    
    if (!p)
        addr = u ? (addr - offset) : (addr + offset);

    if (!p || w)
        *(core->registers[rn]) = addr;
}

void ARMGeneric::PsrTransferMRS(ARMCore *core, uint32_t instr)
{
    bool i = (instr >> 25) & 1;
    bool psr = (instr >> 22) & 1;
    uint8_t rd = (instr >> 12) & 0xF;

    if (psr)
        *(core->registers[rd]) = core->cur_spsr->value;
    else
        *(core->registers[rd]) = core->cpsr.value;
    
    if (core->CanDisassemble)
        printf("mrs r%d, %s_fsxc (0x%08x)\n", rd, psr ? "spsr" : "cpsr", *(core->registers[rd]));
}

void ARMGeneric::PsrTransferMSR(ARMCore *core, uint32_t instr)
{
    bool i = (instr >> 25) & 1;
    bool f = (instr >> 19) & 1;
    bool s = (instr >> 18) & 1;
    bool x = (instr >> 17) & 1;
    bool c = (instr >> 16) & 1;
    bool psr = (instr >> 22) & 1;
    assert(!psr);

    uint32_t operand2;
    std::string op2_disasm;

    if (i)
    {
        uint8_t shamt = (instr & 0xF00) >> 7;
        uint8_t imm = instr & 0xFF;

        operand2 = std::rotr<uint32_t>(imm, shamt);
        op2_disasm = "#" + std::to_string(imm);
        if (shamt)
            op2_disasm += ", #" + std::to_string(shamt);
    }
    else
    {
        uint8_t rm = instr & 0xF;
        operand2 = *(core->registers[rm]);
        op2_disasm = "r" + std::to_string(rm);
    }

    uint32_t mask = 0;
    if (instr & (1 << 19))
        mask |= 0xFF000000;
    if (instr & (1 << 16))
        mask |= 0xFF;
    
    if (core->cpsr.mode == MODE_USER)
        mask &= 0xFFFFFF00;
    if (!psr)
        mask &= 0xFFFFFFDF;
    
    uint32_t value = core->cpsr.value;
    value &= ~mask;
    value |= (operand2 & mask);

    core->SetCPSR(value);

    if (core->CanDisassemble)
    {
        if (!f && !s && !x && !c)
        {
            printf("nop {0}\n");
        }
        else
        {
            printf("msr cpsr_");
            if (f && !s && !x && !c)
                printf("flg");
            else if (f)
                printf("f");
            if (s)
                printf("s");
            if (x)
                printf("x");
            if (c)
                printf("c");
            printf(", %s\n", op2_disasm.c_str());
        }
    }

    core->SwitchMode(core->cpsr.mode);
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

    bool set_carry = false;

    switch (opcode)
    {
        case 0x0:
        case 0x1:
        case 0x8:
        case 0x9:
        case 0xC:
        case 0xD:
        case 0xE:
        case 0xF:
            set_carry = s;
            break;
        default:
            set_carry = false;
            break;
    }

    if (i)
    {
        uint32_t imm = instr & 0xff;
        uint8_t shamt = (instr >> 8) & 0xF;
        shamt <<= 1;

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
            uint8_t rs = (instr >> 8) & 0xF;
            uint8_t rm = instr & 0xF;
            operand2 = *(core->registers[rm]);
            if (rm == 15)
                operand2 += 4;

            uint8_t shamt = *(core->registers[rs]) & 0xFF;
            uint8_t shtype = (instr >> 5) & 0x3;

            if (shamt >= 32)
            {    
                operand2 = 0;
                op2_disasm = ", #0";
                core->cpsr.c = false;
                core->cpsr.z = true;
                core->cpsr.n = false;
            }
            else
            {
                op2_disasm = "r" + std::to_string(rm);
                if (shamt)
                {
                    switch (shtype)
                    {
                    case 0:
                    {
                        if (set_carry)
                            core->cpsr.c = (operand2 >> (32-shamt)) & 1;
                        operand2 <<= shamt;
                        op2_disasm += ", lsl r" + std::to_string(rs);
                        break;
                    }
                    case 1:
                    {
                        if (set_carry)
                            core->cpsr.c = (operand2 >> (shamt-1)) & 1;
                        operand2 >>= shamt;
                        op2_disasm += ", lsr r" + std::to_string(rs);
                        break;
                    }
                    case 2:
                    {
                        if (set_carry)
                            core->cpsr.c = (operand2 >> (shamt-1)) & 1;
                        int32_t tmp = operand2;
                        tmp >>= shamt;
                        operand2 = tmp;
                        op2_disasm += ", asr r" + std::to_string(rs);
                        break;
                    }
                    default:
                        printf("Unknown shift type %d with shamt != 0\n", shtype);
                        exit(1);
                    }
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
        else
        {
            uint8_t rm = instr & 0xF;
            operand2 = *(core->registers[rm]);
            
            uint8_t shamt = (instr >> 7) & 0x1F;
            uint8_t shtype = (instr >> 5) & 0x3;

            assert(shamt < 32);

            op2_disasm = "r" + std::to_string(rm);
            if (shamt)
            {
                switch (shtype)
                {
                case 0:
                {
                    if (set_carry)
                        core->cpsr.c = (operand2 >> (32-shamt)) & 1;
                    operand2 <<= shamt;
                    op2_disasm += ", lsl #" + std::to_string(shamt);
                    break;
                }
                case 1:
                {
                    if (set_carry)
                        core->cpsr.c = (operand2 >> (shamt-1)) & 1;
                    operand2 >>= shamt;
                    op2_disasm += ", lsr #" + std::to_string(shamt);
                    break;
                }
                case 2:
                {
                    if (set_carry)
                        core->cpsr.c = (operand2 >> (shamt-1)) & 1;
                    int32_t tmp = operand2;
                    tmp >>= shamt;
                    operand2 = tmp;
                    op2_disasm += ", asr #" + std::to_string(shamt);
                    break;
                }
                default:
                    printf("Unknown shift type %d with shamt != 0\n", shtype);
                    exit(1);
                }
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
    case 0x00:
    {
        uint32_t result = *(core->registers[rn]) & operand2;


        if (s)
        {
            core->cpsr.z = (result == 0);
            core->cpsr.n = (result >> 31) & 1;
        }

        *(core->registers[rd]) = result;

        if (core->CanDisassemble)
            printf("and%s r%d,r%d,%s\n", s ? "s" : "", rd, rn, op2_disasm.c_str());

        break;
    }
    case 0x01:
    {
        uint32_t result = *(core->registers[rn]) ^ operand2;


        if (s)
        {
            core->cpsr.z = (result == 0);
            core->cpsr.n = (result >> 31) & 1;
        }

        *(core->registers[rd]) = result;

        if (core->CanDisassemble)
            printf("eor%s r%d,r%d,%s\n", s ? "s" : "", rd, rn, op2_disasm.c_str());

        break;
    }
    case 0x02:
    {
        uint32_t result = *(core->registers[rn]) - operand2;

        if (s)
        {
            UpdateFlagsSub(core, *(core->registers[rn]), operand2, result);
        }

        if (s && rd == 15)
        {
            core->cpsr.value = core->cur_spsr->value;
            core->SwitchMode(core->cpsr.mode);
            printf("Returning from interrupt (0x%08x)\n", result);
        }
        
        *(core->registers[rd]) = result;

        if (core->CanDisassemble)
            printf("sub%s r%d, r%d, %s\n", s ? "s" : "", rd, rn, op2_disasm.c_str());
        break;
    }
    case 0x03:
    {
        uint32_t result = operand2 - *(core->registers[rn]);

        if (s)
        {
            UpdateFlagsSub(core, operand2, *(core->registers[rn]), result);
        }
        
        *(core->registers[rd]) = result;

        if (core->CanDisassemble)
            printf("rsb%s r%d, %s\n", s ? "s" : "", rn, op2_disasm.c_str());
        break;
    }
    case 0x04:
    {
        uint32_t result = *(core->registers[rn]) + operand2;

        if (s)
        {
            core->cpsr.n = (result >> 31) & 1;
            core->cpsr.v = CalcOverflowAdd(*(core->registers[rn]), operand2, result);
            core->cpsr.c = (result < *(core->registers[rn]));
            core->cpsr.z = (result == 0);
        }

        *(core->registers[rd]) = result;

        if (core->CanDisassemble)
            printf("add r%d, r%d, %s\n", rd, rn, op2_disasm.c_str());
        break;
    }
    case 0x5:
    {
        uint32_t result = *(core->registers[rn]) + operand2 + core->cpsr.c;
        
        if (s)
        {
            core->cpsr.z = (result == 0);
            core->cpsr.n = (result >> 31) & 1;
            uint32_t temp = *(core->registers[rn]) + operand2;
            uint32_t res = temp + core->cpsr.c;
            core->cpsr.v = CalcOverflowAdd(*(core->registers[rn]), operand2, temp) | CalcOverflowAdd(temp, core->cpsr.c, result);
            core->cpsr.c = ((0xFFFFFFFF-*(core->registers[rn])) < operand2) | ((0xFFFFFFFF-temp) < core->cpsr.c);
        }

        *(core->registers[rd]) = result;

        if (core->CanDisassemble)
            printf("adc%s r%d, r%d, %s\n", s ? "s" : "", rd, rn, op2_disasm.c_str());
        break;
    }
    case 0x6:
    {
        uint32_t result = *(core->registers[rd]) - operand2 - !core->cpsr.c;
        
        if (s)
        {
            UpdateFlagsSbc(core, *(core->registers[rd]), operand2, result);
        }

        *(core->registers[rd]) = result;

        if (core->CanDisassemble)
            printf("sbc%s r%d, r%d, %s\n", s ? "s" : "", rd, rn, op2_disasm.c_str());
        break;
    }
    case 0x07:
    {
        uint32_t result = operand2 - *(core->registers[rn]) + core->cpsr.c - 1;

        if (s)
        {
            UpdateFlagsSbc(core, operand2, *(core->registers[rd]), result);
        }
        
        *(core->registers[rd]) = result;

        if (core->CanDisassemble)
            printf("rsbc%s r%d, %s\n", s ? "s" : "", rn, op2_disasm.c_str());
        break;
    }
    case 0x08:
    {
        uint32_t result = *(core->registers[rn]) & operand2;

        assert(s);

        core->cpsr.z = (result == 0);
        core->cpsr.n = (result >> 31) & 1;

        if (core->CanDisassemble)
            printf("tst r%d,r%d,%s\n", rd, rn, op2_disasm.c_str());

        break;
    }
    case 0x0a:
    {
        uint32_t result = *(core->registers[rn]) - operand2;

        assert(s);

        UpdateFlagsSub(core, *(core->registers[rn]), operand2, result);

        if (core->CanDisassemble)
            printf("cmp r%d, %s\n", rn, op2_disasm.c_str());
        break;
    }
    case 0x0b:
    {
        uint32_t result = *(core->registers[rn]) + operand2;

        assert(s);

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
            printf("orr%s r%d,r%d,%s (0x%08x)\n", s ? "s" : "", rd, rn, op2_disasm.c_str(), result);

        break;
    }
    case 0x0d:
    {
        if (s)
        {
            core->cpsr.z = (operand2 == 0);
            core->cpsr.n = (operand2 >> 31) & 1;
        }

        *(core->registers[rd]) = operand2;

        if (core->CanDisassemble)
            printf("mov%s r%d,%s\n", s ? "s" : "", rd, op2_disasm.c_str());
        break;
    }
    case 0x0e:
    {
        uint32_t result = *(core->registers[rn]) & ~operand2;

        if (s)
        {
            core->cpsr.z = (result == 0);
            core->cpsr.n = (result >> 31) & 1;
        }

        *(core->registers[rd]) = result;

        if (core->CanDisassemble)
            printf("bic%s r%d,r%d,%s (0x%08x)\n", s ? "s" : "", rd, rn, op2_disasm.c_str(), result);

        break;
    }
    case 0x0f:
    {
        if (s)
        {
            core->cpsr.z = ((~operand2) == 0);
            core->cpsr.n = ((~operand2) >> 31) & 1;
        }

        *(core->registers[rd]) = ~operand2;

        if (core->CanDisassemble)
            printf("mvn%s r%d,%s\n", s ? "s" : "", rd, op2_disasm.c_str());
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

void ARMGeneric::MoveToCP(ARMCore *core, uint32_t instr)
{
    uint8_t cpopc = (instr >> 21) & 0x7;
    uint8_t cn = (instr >> 16) & 0xF;
    uint8_t rd = (instr >> 12) & 0xF;
    uint8_t pn = (instr >> 8) & 0xF;
    assert(pn == 15);
    uint8_t cp = (instr >> 5) & 0x7;
    uint8_t cm = instr & 0xF;

    core->cp15->WriteRegister(cpopc, cn, cm, cp, *(core->registers[rd]));

    if (core->CanDisassemble)
        printf("mcr %d, %d, r%d, C%d, C%d, {%d}\n", pn, cpopc, rd, cn, cm, cp);
}

void ARMGeneric::BlxOffset(ARMCore *core, uint32_t instr)
{
    int32_t offs = sign_extend<int32_t>((instr & 0xFFFFFF) << 2, 26);
    bool h = (instr >> 24) & 1;

    *(core->registers[14]) = (*(core->registers[15]) - 4) & ~1;
    *(core->registers[15]) += offs + h*2;
    core->didBranch = true;
    core->cpsr.t = true;

    if (core->CanDisassemble)
        printf("blx 0x%08x\n", *(core->registers[15]));
}

void ARMGeneric::Umull(ARMCore *core, uint32_t instr)
{
    bool s = (instr >> 20) & 1;
    uint8_t rdHi = (instr >> 16) & 0xF;
    uint8_t rdLo = (instr >> 12) & 0xF;
    uint8_t rs = (instr >> 8) & 0xF;
    uint8_t rm = instr & 0xf;

    uint64_t result = *(core->registers[rm]) * *(core->registers[rs]);

    if (s)
    {
        core->cpsr.z = (result == 0);
        core->cpsr.n = (result >> 31) & 1;
    }

    *(core->registers[rdHi]) = (result >> 32);
    *(core->registers[rdLo]) = (uint32_t)result;

    if (core->CanDisassemble)
        printf("umull%s r%d,r%d,r%d,r%d\n", s ? "s" : "", rdLo, rdHi, rm, rs);
}

void ARMGeneric::Mla(ARMCore *core, uint32_t instr)
{
    bool s = (instr >> 20) & 1;
    uint8_t rd = (instr >> 16) & 0xF;
    uint8_t rn = (instr >> 12) & 0xF;
    uint8_t rs = (instr >> 8) & 0xF;
    uint8_t rm = instr & 0xf;

    uint32_t result = *(core->registers[rd]) * *(core->registers[rs]);
    result += *(core->registers[rn]);

    if (s)
    {
        core->cpsr.z = (result == 0);
        core->cpsr.n = (result >> 31) & 1;
    }

    *(core->registers[rd]) = result;

    if (core->CanDisassemble)
        printf("mla%s r%d,r%d,r%d,r%d\n", s ? "s" : "", rd, rm, rs, rn);
}

void ARMGeneric::Clz(ARMCore *core, uint32_t instr)
{
    uint8_t rs = instr & 0xf;
    uint8_t rd = (instr >> 12) & 0xF;

    uint32_t source = *(core->registers[rs]);

    uint32_t bits = 0;
    while ((source & 0xFF000000) == 0)
    {
        bits += 8;
        source <<= 8;
        source |= 0xFF;
    }

    while ((source & 0x80000000) == 0)
    {
        bits++;
        source <<= 1;
        source |= 1;
    }

    *(core->registers[rd]) = bits;

    if (core->CanDisassemble)
        printf("clz r%d,r%d (%d)\n", rd, rs, bits);
}

void ARMGeneric::Wfi(ARMCore *core, uint32_t instr)
{
    (void)instr;
    core->halted = true;
    if (core->CanDisassemble)
        printf("wfi\n");
}

void ARMGeneric::ChangeStateAndMode(ARMCore *core, uint32_t instr)
{
    uint8_t opcode = (instr >> 17) & 0x7;

    bool a = (instr >> 8) & 1;
    bool i = (instr >> 7) & 1;
    bool f = (instr >> 6) & 1;

    switch (opcode)
    {
    case 0b110:
    {
        if (i)
            core->cpsr.i = 1;
        if (f)
            core->cpsr.f = 1;
        if (a)
            core->cpsr.a = 1;
        if (core->CanDisassemble)
            printf("cpsid %s%s%s\n", a ? "a" : "", i ? "i" : "", f ? "f" : "");
        break;
    }
    default:
        printf("Unknown change state/mode opcode 0x%02x\n", opcode);
        exit(1);
    }
}

void ARMGeneric::DoARMInstruction(ARMCore *core, uint32_t instr)
{
    if (core->CanDisassemble)
        printf("[ARM%d]: ", core->id);

    if (((instr >> 28) & 0xF) == 0xF)
    {
        if (IsBlxOffset(instr))
        {
            BlxOffset(core, instr);
            return;
        }
        else if (IsModeFlagChange(instr))
        {
            ChangeStateAndMode(core, instr);
            return;
        }

        printf("Unhandled extended ARM instruction 0x%08x\n", instr);
        exit(1);
    }

    if (!CondPassed(core->cpsr, (instr >> 28) & 0xF))
    {
        if (core->CanDisassemble)
            printf("Cond failed\n");
        return;
    }

    if (IsBranchExchange(instr))
    {
        BranchExchange(core, instr);
        return;
    }
    else if (IsBlockDataTransfer(instr))
    {
        BlockDataTransfer(core, instr);
        return;
    }
    else if (IsBranch(instr))
    {
        Branch(core, instr);
        return;
    }
    else if (IsSingleDataTransfer(instr))
    {
        SingleDataTransfer(core, instr);
        return;
    }
    else if (IsWFI(instr))
    {
        Wfi(core, instr);
        return;
    }
    else if (IsBlxReg(instr))
    {
        BlxReg(core, instr);
        return;
    }
    else if (IsPSRTransferMSR(instr))
    {
        PsrTransferMSR(core, instr);
        return;
    }
    else if (IsPSRTransferMRS(instr))
    {
        PsrTransferMRS(core, instr);
        return;
    }
    else if (IsUMULL(instr))
    {
        Umull(core, instr);
        return;
    }
    else if (IsMLA(instr))
    {
        Mla(core, instr);
        return;
    }
    else if (IsCLZ(instr))
    {
        Clz(core, instr);
        return;
    }
    else if (IsHalfWordTransferReg(instr))
    {
        HalfwordDataTransferReg(core, instr);
        return;
    }
    else if (IsHalfWordTransferImm(instr))
    {
        HalfwordDataTransferImm(core, instr);
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
    else if (IsMCR(instr))
    {
        MoveToCP(core, instr);
        return;
    }

    printf("Unhandled ARM instruction 0x%08x\n", instr);
    exit(1);
}
