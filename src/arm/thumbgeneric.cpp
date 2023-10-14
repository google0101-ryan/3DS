#include "armgeneric.h"

#include <string>
#include <algorithm>
#include <cassert>
#include <bitset>

extern bool CondPassed(CPSR&, uint8_t);

const char* GetCondName(uint8_t cond)
{
    switch (cond)
    {
    case 0: return "eq";
    case 1: return "ne";
    case 2: return "cs";
    case 3: return "cc";
    case 4: return "mi";
    case 5: return "pl";
    case 6: return "vs";
    case 7: return "vc";
    case 8: return "hi";
    case 9: return "ls";
    case 10: return "ge";
    case 11: return "lt";
    case 12: return "gt";
    case 13: return "le";
    }

    return "UNDEFINED";
}

bool IsPushPop(uint16_t instr)
{
    return ((instr >> 12) & 0xF) == 0xB
            && ((instr >> 9) & 0x3) == 0x2;
}

bool IsPCRelativeLoad(uint16_t instr)
{
    return ((instr >> 11) & 0x1F) == 0x9;
}

bool IsLongBranchFirstHalf(uint16_t instr)
{
    return ((instr >> 11) & 0x1F) == 0x1E;
}

bool IsLongBranchSecondHalf(uint16_t instr)
{
    return ((instr >> 11) & 0x1F) == 0x1F;
}

bool IsLongBranchExchange(uint16_t instr)
{
    return ((instr >> 11) & 0x1F) == 0x1D;
}

bool IsLoadStoreImm(uint16_t instr)
{
    return ((instr >> 13) & 0x7) == 0x3;
}

bool IsLoadStoreReg(uint16_t instr)
{
    return ((instr >> 12) & 0xF) == 0x5;
}

bool IsMovCmpAddSub(uint16_t instr)
{
    return ((instr >> 13) & 0x7) == 1;
}

bool IsConditionalBranch(uint16_t instr)
{
    return ((instr >> 12) & 0xF) == 0xD;
}

bool IsHiRegisterOp(uint16_t instr)
{
    return ((instr >> 10) & 0x3F) == 0x11;
}

bool IsAddSub(uint16_t instr)
{
    return ((instr >> 11) & 0x1F) == 0x3;
}

bool IsMoveShifted(uint16_t instr)
{
    return ((instr >> 13) & 0x7) == 0;
}

bool IsAddSubSP(uint16_t instr)
{
    return ((instr >> 8) & 0xFF) == 0xB0;
}

bool IsALUOperation(uint16_t instr)
{
    return ((instr >> 10) & 0x3F) == 0x10;
}

bool IsUnconditionalBranch(uint16_t instr)
{
    return ((instr >> 11) & 0x1F) == 0x1C;
}

bool IsLoadStoreHalfword(uint16_t instr)
{
    return ((instr >> 12) & 0xF) == 0x8;
}

bool IsPCSPRelative(uint16_t instr)
{
    return ((instr >> 12) & 0xF) == 0xA;
}

bool IsSignedUnsignedExtend(uint16_t instr)
{
    return ((instr >> 8) & 0xFF) == 0xB2;
}

bool IsSPRelativeLoadStore(uint16_t instr)
{
    return ((instr >> 12) & 0xF) == 0x9;
}

bool IsThumbLDMSTM(uint16_t instr)
{
    return ((instr >> 12) & 0xF) == 0xC;
}

void ARMGeneric::DoTHUMBInstruction(ARMCore* core, uint16_t instr)
{
    if (core->CanDisassemble)
        printf("[ARM%d]: ", core->id);

    if (IsPushPop(instr))
    {
        PushPop(core, instr);
        return;
    }
    else if (IsPCRelativeLoad(instr))
    {
        PCRelativeLoad(core, instr);
        return;
    }
    else if (IsLongBranchFirstHalf(instr))
    {
        LongBranchFirstHalf(core, instr);
        return;
    }
    else if (IsLongBranchSecondHalf(instr))
    {
        LongBranchSecondHalf(core, instr);
        return;
    }
    else if (IsLongBranchExchange(instr))
    {
        LongBranchExchange(core, instr);
        return;
    }
    else if (IsThumbLDMSTM(instr))
    {
        ThumbLDMSTM(core, instr);
        return;
    }
    else if (IsLoadStoreImm(instr))
    {
        LoadStoreImmOffs(core, instr);
        return;
    }
    else if (IsLoadStoreReg(instr))
    {
        LoadStoreRegOffs(core, instr);
        return;
    }
    else if (IsMovCmpAddSub(instr))
    {
        MovCmpAddSub(core, instr);
        return;
    }
    else if (IsConditionalBranch(instr))
    {
        ConditionalBranch(core, instr);
        return;
    }
    else if (IsHiRegisterOp(instr))
    {
        HiRegisterOps(core, instr);
        return;
    }
    else if (IsAddSub(instr))
    {
        DoAddSub(core, instr);
        return;
    }
    else if (IsMoveShifted(instr))
    {
        DoShift(core, instr);
        return;
    }
    else if (IsAddSubSP(instr))
    {
        AddSubSP(core, instr);
        return;
    }
    else if (IsALUOperation(instr))
    {
        ALUOperations(core, instr);
        return;
    }
    else if (IsUnconditionalBranch(instr))
    {
        UnconditionalBranch(core, instr);
        return;
    }
    else if (IsLoadStoreHalfword(instr))
    {
        LoadStoreHalfword(core, instr);
        return;
    }
    else if (IsPCSPRelative(instr))
    {
        PCSPOffset(core, instr);
        return;
    }
    else if (IsSignedUnsignedExtend(instr))
    {
        SignedUnsignedExtend(core, instr);
        return;
    }
    else if (IsSPRelativeLoadStore(instr))
    {
        SPRelativeLoadStore(core, instr);
        return;
    }

    printf("Unhandled THUMB instruction 0x%04x\n", instr);
    exit(1);
}

void ARMGeneric::PushPop(ARMCore *core, uint16_t instr)
{
    bool l = (instr >> 11) & 1;
    bool pc = (instr >> 8) & 1;
    uint8_t rlist = instr & 0xFF;

    uint32_t addr = *(core->registers[13]);

    if (l)
    {
        for (int i = 0; i < 8; i++)
        {
            if (rlist & (1 << i))
            {
                *(core->registers[i]) = core->Read32(addr);
                addr += 4;
            }
        }

        if (pc)
        {
            *(core->registers[15]) = core->Read32(addr);
            addr += 4;
            core->didBranch = true;
            if (!(*(core->registers[15]) & 1))
                core->cpsr.t = false;
            *(core->registers[15]) &= ~1;
        }
    }
    else
    {
        if (pc)
        {
            addr -= 4;
            core->Write32(addr, *(core->registers[14]));
        }

        for (int i = 7; i >= 0; i--)
        {
            if (rlist & (1 << i))
            {
                addr -= 4;
                core->Write32(addr, *(core->registers[i]));
            }
        }
    }

    *(core->registers[13]) = addr;

    if (core->CanDisassemble)
    {
        int count = std::bitset<8>(rlist).count();

        bool print_comma = true;
        int num_printed = 0;
        printf("%s {", l ? "pop" : "push");
        for (int i = 0; i < 8; i++)
            if (rlist & (1 << i))
            {
                if (num_printed == (count-1))
                    print_comma = false;
                printf("r%d%s", i, print_comma ? ", " : "");
                num_printed++;
            }
        
        if (pc)
            printf(", %s", l ? "pc" : "lr");

        printf("}\n");
    }
}

void ARMGeneric::PCRelativeLoad(ARMCore *core, uint16_t instr)
{
    uint32_t offset = (instr & 0xFF) << 2;
    uint8_t rd = (instr >> 8) & 0x7;

    *(core->registers[rd]) = core->Read32((*(core->registers[15]) & ~2) + offset);

    if (core->CanDisassemble)
        printf("ldr r%d, [pc, #%d]\n", rd, offset);
}

void ARMGeneric::LongBranchFirstHalf(ARMCore *core, uint16_t instr)
{
    int32_t offset = ((instr & 0x7FF) << 21) >> 9;
    uint32_t target = *(core->registers[15]) + offset;
    *(core->registers[14]) = target;

    if (core->CanDisassemble)
        printf("0x%08x (0x%08x)\n", target, offset);
}

void ARMGeneric::LongBranchSecondHalf(ARMCore *core, uint16_t instr)
{
    uint32_t offset = instr & 0x7FF;
    uint32_t target_lr = *(core->registers[15]) - 2;
    *(core->registers[15]) = *(core->registers[14]) + (offset << 1);
    *(core->registers[14]) = target_lr | 1;

    core->didBranch = true;

    if (core->CanDisassemble)
        printf("bl 0x%08x\n", *(core->registers[15]));
}

void ARMGeneric::LongBranchExchange(ARMCore *core, uint16_t instr)
{
    uint32_t offset = instr & 0x7FF;
    uint32_t target_lr = *(core->registers[15]) - 2;
    *(core->registers[15]) = (*(core->registers[14]) + (offset << 1)) & ~3;
    *(core->registers[14]) = target_lr | 1;
    core->cpsr.t = 0;

    core->didBranch = true;

    if (core->CanDisassemble)
        printf("blx 0x%08x (0x%08x)\n", *(core->registers[15]), target_lr);
}

void ARMGeneric::LoadStoreImmOffs(ARMCore *core, uint16_t instr)
{
    bool l = (instr >> 11) & 1;
    bool b = (instr >> 12) & 1;
    uint32_t offset = (instr >> 6) & 0x1F;
    if (!b)
        offset <<= 2;
    
    uint8_t rb = (instr >> 3) & 0x7;
    uint8_t rd = instr & 0x7;

    uint32_t addr = *(core->registers[rb]) + offset;

    if (l)
    {
        if (b)
            *(core->registers[rd]) = core->Read8(addr);
        else
            *(core->registers[rd]) = core->Read32(addr);
    }
    else
    {
        if (b)
            core->Write8(addr, *(core->registers[rd]) & 0xFF);
        else
        {
            core->Write32(addr & ~3, *(core->registers[rd]));
        }
    }

    if (core->CanDisassemble)
        printf("%s%s r%d, [r%d, #%d]\n", l ? "ldr" : "str", b ? "b" : "", rd, rb, offset);
}

void ARMGeneric::LoadStoreRegOffs(ARMCore *core, uint16_t instr)
{
    bool l = (instr >> 11) & 1;
    bool b = (instr >> 10) & 1;
    uint8_t ro = (instr >> 6) & 0x7;
    uint8_t rb = (instr >> 3) & 0x7;
    uint8_t rd = instr & 0x7;

    uint32_t addr = *(core->registers[rb]) + *(core->registers[ro]);

    if (l)
    {
        if (b)
            *(core->registers[rd]) = core->Read8(addr);
        else
            *(core->registers[rd]) = core->Read32(addr);
    }
    else
    {
        if (b)
            core->Write8(addr, *(core->registers[rd]));
        else
            core->Write32(addr, *(core->registers[rd]));
    }

    if (core->CanDisassemble)
        printf("%s%s r%d, [r%d, r%d]\n", l ? "ldr" : "str", b ? "b" : "", rd, rb, ro);
}

void ARMGeneric::MovCmpAddSub(ARMCore *core, uint16_t instr)
{
    uint8_t opcode = (instr >> 11) & 0x3;
    uint8_t rd = (instr >> 8) & 0x7;
    uint8_t imm = instr & 0xFF;

    switch (opcode)
    {
    case 0:
    {
        core->cpsr.z = (imm == 0);
        core->cpsr.n = (imm >> 31) & 1;

        *(core->registers[rd]) = imm;

        if (core->CanDisassemble)
            printf("movs r%d, #%d\n", rd, imm);
        break;
    }
    case 1:
    {
        uint32_t result = *(core->registers[rd]) - imm;

        UpdateFlagsSub(core, *(core->registers[rd]), imm, result);

        if (core->CanDisassemble)
            printf("cmp r%d, #%d (%d)\n", rd, imm, result);
        break;
    }
    case 2:
    {
        uint32_t result = *(core->registers[rd]) + imm;

        core->cpsr.n = (result >> 31) & 1;
        core->cpsr.v = CalcOverflowAdd(*(core->registers[rd]), imm, result);
        core->cpsr.c = (result < *(core->registers[rd]));
        core->cpsr.z = (result == 0);

        *(core->registers[rd]) = result;

        if (core->CanDisassemble)
            printf("adds r%d, #%d (0x%08x)\n", rd, imm, result);
        break;
    }
    case 3:
    {
        uint32_t result = *(core->registers[rd]) - imm;

        UpdateFlagsSub(core, *(core->registers[rd]), imm, result);

        *(core->registers[rd]) = result;

        if (core->CanDisassemble)
            printf("subs r%d, #%d\n", rd, imm);
        break;
    }
    default:
        printf("Unknown mov/cmp/add/sub opcode %d\n", opcode);
        exit(1);
    }
}

void ARMGeneric::ConditionalBranch(ARMCore *core, uint16_t instr)
{
    int32_t off = sign_extend<int32_t>((instr & 0xFF) << 1, 9);
    uint8_t cond = (instr >> 8) & 0xF;

    assert(cond != 0xE && cond != 0xF);

    if (core->CanDisassemble)
        printf("b%s 0x%08x ", GetCondName(cond), *(core->registers[15]) + off);
    
    if (CondPassed(core->cpsr, cond))
    {
        core->didBranch = true;
        *(core->registers[15]) += off;
        if (*(core->registers[15]) == 0xffff3df8)
            exit(1);
        if (core->CanDisassemble)
            printf("[TAKEN]");
    }
    if (core->CanDisassemble)
        printf("\n");
}

void ARMGeneric::HiRegisterOps(ARMCore *core, uint16_t instr)
{
    uint8_t opcode = (instr >> 8) & 0x3;
    bool hd = (instr >> 7) & 1;
    bool hs = (instr >> 6) & 1;
    uint8_t rs = (instr >> 3) & 0x7;
    uint8_t rd = instr & 0x7;

    if (hd)
        rd += 8;
    if (hs)
        rs += 8;

    switch (opcode)
    {
    case 1:
    {
        uint32_t result = *(core->registers[rd]) - *(core->registers[rs]);

        UpdateFlagsSub(core, *(core->registers[rd]), *(core->registers[rs]), result);

        if (core->CanDisassemble)
            printf("cmp r%d, r%d (%d)\n", rd, rs, result);
        break;
    }
    case 2:
    {
        *(core->registers[rd]) = *(core->registers[rs]);
        if (core->CanDisassemble && rd == 8 && rs == 8)
            printf("nop\n");
        else if (core->CanDisassemble)
            printf("mov r%d, r%d\n", rd, rs);
        break;
    }
    case 3:
    {
        uint32_t target = *(core->registers[rs]);
        if (hd)
            *(core->registers[14]) = (*(core->registers[15]) - 2) | 1;
        *(core->registers[15]) = target & ~1;
        core->cpsr.t = target & 1;
        core->didBranch = true;
        if (core->CanDisassemble)
            printf("b%sx r%d\n", hd ? "l" : "", rs);
        break;
    }
    default:
        printf("Unknown hi register op 0x%02x\n", opcode);
        exit(1);
    }
}

void ARMGeneric::DoAddSub(ARMCore *core, uint16_t instr)
{
    bool s = (instr >> 9) & 1;
    bool i = (instr >> 10) & 1;
    uint8_t rs = (instr >> 3) & 0x7;
    uint8_t rd = instr & 0x7;

    uint32_t operand2;
    std::string op2_disasm;

    if (i)
    {
        operand2 = (instr >> 6) & 0x7;
        op2_disasm = "#" + std::to_string(operand2);
    }
    else
    {
        uint8_t rn = (instr >> 6) & 0x7;
        operand2 = *(core->registers[rn]);
        op2_disasm = "r" + std::to_string(rn);
    }

    if (s)
    {
        uint32_t result = *(core->registers[rs]) - operand2;

        UpdateFlagsSub(core, *(core->registers[rs]), operand2, result);

        *(core->registers[rd]) = result;

        if (core->CanDisassemble)
            printf("sub r%d, r%d, %s (%d)\n", rd, rs, op2_disasm.c_str(), result);
    }
    else
    {
        uint32_t result = *(core->registers[rs]) + operand2;

        core->cpsr.z = (result == 0);
        core->cpsr.n = (result >> 31) & 1;
        core->cpsr.v = CalcOverflowAdd(*(core->registers[rs]), operand2, result);
        core->cpsr.c = (result < *(core->registers[rs]));

        *(core->registers[rd]) = result;

        if (core->CanDisassemble)
            printf("add r%d, r%d, %s\n", rd, rs, op2_disasm.c_str());
    }
}

void ARMGeneric::DoShift(ARMCore *core, uint16_t instr)
{
    uint8_t opcode = (instr >> 11) & 0x3;
    uint8_t offs = (instr >> 6) & 0x1F;
    uint8_t rs = (instr >> 3) & 0x7;
    uint8_t rd = instr & 0x7;

    if (!offs)
    {
        switch (opcode)
        {
        case 0:
        {
            uint32_t result = *(core->registers[rs]);
            core->cpsr.z = (result == 0);
            core->cpsr.n = (result >> 31) & 1;
            
            *(core->registers[rd]) = result;
            if (core->CanDisassemble)
                printf("movs r%d, r%d (0x%08x)\n", rd, rs, result);
            break;
        }
        default:
            printf("Unknown shift by 0 operation %d\n", opcode);
            exit(1);
        }
    }
    else
    {
        switch (opcode)
        {
        case 0:
        {
            assert(offs < 32);
            uint32_t result = *(core->registers[rs]) << offs;
            core->cpsr.z = (result == 0);
            core->cpsr.n = (result >> 31) & 1;
            core->cpsr.c = (*(core->registers[rs]) >> (32-offs)) & 1;
            
            *(core->registers[rd]) = result;
            if (core->CanDisassemble)
                printf("lsls r%d, r%d, #%d (0x%08x)\n", rd, rs, offs, result);
            break;
        }
        case 1:
        {
            assert(offs < 32);
            uint32_t result = (*(core->registers[rs])) >> offs;
            core->cpsr.z = (result == 0);
            core->cpsr.n = (result >> 31) & 1;
            core->cpsr.c = (*(core->registers[rs]) >> (offs-1)) & 1;
            
            *(core->registers[rd]) = result;
            if (core->CanDisassemble)
                printf("lsrs r%d, r%d, #%d\n", rd, rs, offs);
            break;
        }
        case 2:
        {
            assert(offs < 32);
            uint32_t result = ((int32_t)*(core->registers[rs])) >> offs;
            core->cpsr.z = (result == 0);
            core->cpsr.n = (result >> 31) & 1;
            core->cpsr.c = (*(core->registers[rs]) >> (offs-1)) & 1;
            
            *(core->registers[rd]) = result;
            if (core->CanDisassemble)
                printf("asrs r%d, r%d, #%d\n", rd, rs, offs);
            break;
        }
        default:
            printf("Unknown shift operation %d\n", opcode);
            exit(1);
        }
    }
}

void ARMGeneric::AddSubSP(ARMCore *core, uint16_t instr)
{
    bool sign = (instr >> 7) & 1;
    int imm = (instr & 0x7F) << 2;

    if (sign)
        imm = -imm;
    
    *(core->registers[13]) += imm;

    if (core->CanDisassemble)
        printf("%s sp,#%d\n", imm < 0 ? "sub" : "add", imm < 0 ? -imm : imm);
}

void ARMGeneric::ALUOperations(ARMCore *core, uint16_t instr)
{
    uint8_t opcode = (instr >> 6) & 0xF;
    uint8_t rs = (instr >> 3) & 0x7;
    uint8_t rd = instr & 0x7;

    switch (opcode)
    {
    case 0x0:
    {
        uint32_t result = *(core->registers[rd]) & *(core->registers[rs]);
        
        core->cpsr.z = (result == 0);
        core->cpsr.n = (result >> 31) & 1;

        *(core->registers[rd]) = result;
        
        if (core->CanDisassemble)
            printf("ands r%d, r%d\n", rd, rs);
        break;
    }
    case 0x1:
    {
        uint32_t result = *(core->registers[rd]) ^ *(core->registers[rs]);
        
        core->cpsr.z = (result == 0);
        core->cpsr.n = (result >> 31) & 1;

        *(core->registers[rd]) = result;
        
        if (core->CanDisassemble)
            printf("eors r%d, r%d\n", rd, rs);
        break;
    }
    case 2:
    {
        if ((*(core->registers[rs]) & 0xFF) >= 32)
        {
            *(core->registers[rd]) = 0;
            core->cpsr.c = false;
            core->cpsr.z = true;
            core->cpsr.n = false;
        }
        else if ((*(core->registers[rs]) & 0xFF) != 0)
        {
            uint32_t result = *(core->registers[rd]) << (*(core->registers[rs]) & 0xFF);
            core->cpsr.z = (result == 0);
            core->cpsr.n = (result >> 31) & 1;
            core->cpsr.c = (*(core->registers[rd]) >> (32-(*(core->registers[rs]) & 0xFF))) & 1;
                
            *(core->registers[rd]) = result;
        }
        else
        {
            uint32_t result = *(core->registers[rd]);
            core->cpsr.z = (result == 0);
            core->cpsr.n = (result >> 31) & 1;
        }
        if (core->CanDisassemble)
            printf("lsls r%d, r%d, r%d\n", rd, rs, rs);
        break;
    }
    case 3:
    {
        if ((*(core->registers[rs]) & 0xFF) >= 32)
        {
            *(core->registers[rd]) = 0;
            core->cpsr.c = false;
            core->cpsr.z = true;
            core->cpsr.n = false;
        }
        else if ((*(core->registers[rs]) & 0xFF) != 0)
        {
            uint32_t result = *(core->registers[rd]) >> (*(core->registers[rs]) & 0xFF);
            core->cpsr.z = (result == 0);
            core->cpsr.n = (result >> 31) & 1;
            core->cpsr.c = (*(core->registers[rd]) >> ((*(core->registers[rs]) & 0xFF)-1)) & 1;
                
            *(core->registers[rd]) = result;
        }
        else
        {
            assert(0);
        }
        if (core->CanDisassemble)
            printf("lsrs r%d, r%d, r%d\n", rd, rd, rs);
        break;
    }
    case 0x5:
    {
        uint32_t result = *(core->registers[rd]) + *(core->registers[rs]) + core->cpsr.c;
        
        core->cpsr.z = (result == 0);
        core->cpsr.n = (result >> 31) & 1;
        uint32_t temp = *(core->registers[rd]) + *(core->registers[rs]);
        uint32_t res = temp + core->cpsr.c;
        core->cpsr.v = CalcOverflowAdd(*(core->registers[rd]), *(core->registers[rs]), temp) | CalcOverflowAdd(temp, core->cpsr.c, result);
        core->cpsr.c = ((0xFFFFFFFF-*(core->registers[rd])) < *(core->registers[rs])) | ((0xFFFFFFFF-temp) < core->cpsr.c);

        *(core->registers[rd]) = result;

        if (core->CanDisassemble)
            printf("adcs r%d, r%d\n", rd, rs);
        break;
    }
    case 0x6:
    {
        uint32_t result = *(core->registers[rd]) - *(core->registers[rs]) - !core->cpsr.c;
        
        UpdateFlagsSbc(core, *(core->registers[rd]), *(core->registers[rs]), result);

        *(core->registers[rd]) = result;

        if (core->CanDisassemble)
            printf("sbcs r%d, r%d\n", rd, rs);
        break;
    }
    case 0x8:
    {
        uint32_t result = *(core->registers[rd]) & *(core->registers[rs]);
        
        core->cpsr.z = (result == 0);
        core->cpsr.n = (result >> 31) & 1;
        
        if (core->CanDisassemble)
            printf("tst r%d, r%d\n", rd, rs);
        break;
    }
    case 0xA:
    {
        uint32_t result = *(core->registers[rd]) - *(core->registers[rs]);
        
        UpdateFlagsSub(core, *(core->registers[rd]), *(core->registers[rs]), result);

        if (core->CanDisassemble)
            printf("cmp r%d, r%d (0x%08x, 0x%08x)\n", rd, rs, *(core->registers[rd]), *(core->registers[rs]));
        break;
    }
    case 0xC:
    {
        uint32_t result = *(core->registers[rd]) | *(core->registers[rs]);
        
        core->cpsr.z = (result == 0);
        core->cpsr.n = (result >> 31) & 1;

        *(core->registers[rd]) = result;
        
        if (core->CanDisassemble)
            printf("orrs r%d, r%d\n", rd, rs);
        break;
    }
    case 0xD:
    {
        uint32_t result = *(core->registers[rd]) * *(core->registers[rs]);
        
        core->cpsr.z = (result == 0);
        core->cpsr.n = (result >> 31) & 1;

        *(core->registers[rd]) = result;
        
        if (core->CanDisassemble)
            printf("muls r%d, r%d\n", rd, rs);
        break;
    }
    case 0xE:
    {
        uint32_t result = *(core->registers[rd]) & ~(*(core->registers[rs]));
        
        core->cpsr.z = (result == 0);
        core->cpsr.n = (result >> 31) & 1;

        *(core->registers[rd]) = result;
        
        if (core->CanDisassemble)
            printf("bics r%d, r%d\n", rd, rs);
        break;
    }
    case 0xF:
    {
        uint32_t result = ~(*(core->registers[rs]));
        
        core->cpsr.z = (result == 0);
        core->cpsr.n = (result >> 31) & 1;
        
        *(core->registers[rd]) = result;
        
        if (core->CanDisassemble)
            printf("mvn r%d, r%d\n", rd, rs);
        break;
    }
    default:
        printf("Unknown ALU operation %d\n", opcode);
        exit(1);
    }
}

void ARMGeneric::UnconditionalBranch(ARMCore *core, uint16_t instr)
{
    int32_t offs = sign_extend<int32_t>((instr & 0x7FF) << 1, 12);

    *(core->registers[15]) += offs;
    core->didBranch = true;

    if (core->CanDisassemble)
        printf("b 0x%08x\n", *(core->registers[15]));
}

void ARMGeneric::LoadStoreHalfword(ARMCore *core, uint16_t instr)
{
    bool l = (instr >> 11) & 1;
    uint32_t offset = ((instr >> 6) & 0x1F) << 1;
    uint8_t rb = (instr >> 3) & 0x7;
    uint8_t rd = instr & 0x7;

    uint32_t addr = *(core->registers[rb]) + offset;

    if (l)
    {
        *(core->registers[rd]) = core->Read16(addr & ~1);
    }
    else
        core->Write16(addr & ~1, *(core->registers[rd]));
    
    if (core->CanDisassemble)
        printf("%s r%d, [r%d, #%d]\n", l ? "ldrh" : "strh", rd, rb, offset);
}

void ARMGeneric::PCSPOffset(ARMCore *core, uint16_t instr)
{
    bool source = (instr >> 11) & 1;
    uint8_t rd = (instr >> 8) & 0x7;
    uint32_t offset = (instr & 0xFF) << 2;

    uint32_t source_data;
    if (source)
        source_data = *(core->registers[13]);
    else
        source_data = *(core->registers[15]) & ~2;
    
    *(core->registers[rd]) = source_data + offset;

    if (core->CanDisassemble)
        printf("add r%d,%s,#%d\n", rd, source ? "sp" : "pc", offset);
}

void ARMGeneric::SignedUnsignedExtend(ARMCore *core, uint16_t instr)
{
    uint8_t opcode = (instr >> 6) & 0x3;
    uint8_t rm = (instr >> 3) & 0x7;
    uint8_t rd = instr & 0x7;
    
    switch (opcode)
    {
    case 0:
    {
        uint32_t data = (int32_t)(int16_t)(uint16_t)*(core->registers[rm]);
        *(core->registers[rd]) = data;
        if (core->CanDisassemble)
            printf("sxth r%d,r%d\n", rd, rm);
        break;
    }
    case 1:
    {
        uint32_t data = (int32_t)(int8_t)(uint8_t)*(core->registers[rm]);
        *(core->registers[rd]) = data;
        if (core->CanDisassemble)
            printf("sxtb r%d,r%d\n", rd, rm);
        break;
    }
    case 2:
    {
        uint32_t data = (uint16_t)*(core->registers[rm]);
        *(core->registers[rd]) = data;
        if (core->CanDisassemble)
            printf("uxth r%d,r%d\n", rd, rm);
        break;
    }
    case 3:
    {
        uint32_t data = (uint8_t)*(core->registers[rm]);
        *(core->registers[rd]) = data;
        if (core->CanDisassemble)
            printf("uxtb r%d,r%d\n", rd, rm);
        break;
    }
    }
}

void ARMGeneric::SPRelativeLoadStore(ARMCore *core, uint16_t instr)
{
    bool l = (instr >> 11) & 1;
    uint8_t rd = (instr >> 8) & 0x7;
    uint32_t offs = (instr & 0xff) << 2;

    uint32_t addr = *(core->registers[13]) + offs;

    if (l)
        *(core->registers[rd]) = core->Read32(addr);
    else
        core->Write32(addr, *(core->registers[rd]));
    
    if (core->CanDisassemble)
    {
        printf("%s r%d, [sp, #%d] (0x%08x)\n", l ? "ldr" : "str", rd, offs, *(core->registers[rd]));
    }
}

void ARMGeneric::ThumbLDMSTM(ARMCore *core, uint16_t instr)
{
    bool l = (instr >> 11) & 1;
    uint8_t rb = (instr >> 8) & 0x7;
    uint8_t rlist = instr & 0xff;

    uint32_t address = *(core->registers[rb]);

    if (l)
    {
        for (int i = 0; i < 8; i++)
        {
            if (rlist & (1 << i))
            {
                *(core->registers[i]) = core->Read32(address & ~3);
                address += 4;
            }
        }
    }
    else
    {
        for (int i = 0; i < 8; i++)
        {
            if (rlist & (1 << i))
            {
                core->Write32(address & ~3, *(core->registers[i]));
                address += 4;
            }
        }
    }

    if (!l || !(rlist & (1 << rb)))
        *(core->registers[rb]) = address;

    if (core->CanDisassemble)
    {
        printf("%s r%d, {", l ? "ldm" : "stm", rb);
        bool notFirstTime = false;
        for (int i = 0; i < 8; i++)
        {
            if (!(rlist & (1 << i)))
                continue;

            if (!notFirstTime)
            {
                notFirstTime = true;
                printf("r%d", i);
            }
            else
                printf(", r%d", i);
        }
        printf("}\n");
    }
}

#define ADD_OVERFLOW(a, b, result) ((!(((a) ^ (b)) & 0x80000000)) && (((a) ^ (result)) & 0x80000000))
#define SUB_OVERFLOW(a, b, result) (((a) ^ (b)) & 0x80000000) && (((a) ^ (result)) & 0x80000000)
#define CARRY_SUB(a, b) (a > b)

void ARMGeneric::UpdateFlagsSub(ARMCore* core, uint32_t source, uint32_t operand2, uint32_t result)
{
    uint64_t unsigned_result = source - operand2;

    core->cpsr.n = (result >> 31) & 1;
    core->cpsr.z = (result == 0);
    core->cpsr.c = !(operand2 > source);
    core->cpsr.v = SUB_OVERFLOW(source, operand2, result);
}

void ARMGeneric::UpdateFlagsSbc(ARMCore *core, uint32_t source, uint32_t operand2, uint32_t result)
{
    unsigned int borrow = core->cpsr.c ? 0 : 1;
    UpdateFlagsSub(core, source, operand2+borrow, result);
    uint32_t temp = source - operand2;
    uint32_t res = temp - borrow;
    core->cpsr.c = !(CARRY_SUB(source, operand2) & CARRY_SUB(temp, borrow));
    core->cpsr.v = SUB_OVERFLOW(source, operand2, temp) | SUB_OVERFLOW(temp, borrow, res);
}
