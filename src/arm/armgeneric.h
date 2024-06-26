#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <bit>

#include "cp15.h"

#define ADD_OVERFLOW(a, b, result) ((!(((a) ^ (b)) & 0x80000000)) && (((a) ^ (result)) & 0x80000000))
#define SUB_OVERFLOW(a, b, result) (((a) ^ (b)) & 0x80000000) && (((a) ^ (result)) & 0x80000000)
#define CARRY_SUB(a, b) (a > b)
#define CARRY_ADD(a, b)  ((0xFFFFFFFF-a) < b)

template<typename T>
inline T sign_extend(T x, int bits)
{
    T m = 1;
    m <<= bits - 1;
    return (x ^ m) - m;
}

inline bool CalcOverflowSub(uint32_t a, uint32_t b, uint32_t result)
{
    bool sA = (a >> 31) & 1;
    bool sB = (b >> 31) & 1;
    bool sR = (result >> 31) & 1;
    return sA != sB && sB == sR;
}

inline bool CalcOverflowAdd(uint32_t a, uint32_t b, uint32_t result)
{
    bool sA = (a >> 31) & 1;
    bool sB = (b >> 31) & 1;
    bool sR = (result >> 31) & 1;
    return sA == sB && sB != sR;
}

enum ArmModes
{
    MODE_USER = 0x10,
    MODE_IRQ = 0x12,
    MODE_SUPERVISOR = 0x13,
    MODE_SYSTEM = 0x1F
};

union CPSR
{
    uint32_t value;
    struct
    {
        uint32_t mode : 5;
        uint32_t t : 1;
        uint32_t f : 1;
        uint32_t i : 1;
        uint32_t a : 1;
        uint32_t e : 1;
        uint32_t rsv1 : 14;
        uint32_t j : 1;
        uint32_t rsv0 : 2;
        uint32_t q : 1;
        uint32_t v : 1;
        uint32_t c : 1;
        uint32_t z : 1;
        uint32_t n : 1;
    };
};

class ARMCore
{
    friend class ARMGeneric;
protected:
    uint32_t* registers[16];
    uint32_t regs[16];
    uint32_t regs_svc[2];
    uint32_t regs_fiq[7];
    uint32_t regs_abt[2];
    uint32_t regs_irq[2];
    uint32_t regs_und[2];

    CPSR cpsr, *cur_spsr, spsr_svc, spsr_irq;

    CP15* cp15 = nullptr;

    bool CanDisassemble = true;
    bool didBranch = false;
    bool halted = false;

    int id; // Either 11, 9, or 7 depending on the ARM core

    uint32_t pipeline[2];
    uint16_t t_pipeline[2];

    void SetCPSR(uint32_t value)
    {
        cpsr.n = (value >> 31) & 1;
        cpsr.z = (value >> 30) & 1;
        cpsr.z = (value >> 29) & 1;
        cpsr.z = (value >> 28) & 1;
        cpsr.z = (value >> 27) & 1;

        cpsr.i = (value >> 7) & 1;
        cpsr.f = (value >> 6) & 1;
        cpsr.t = (value >> 5) & 1;

        cpsr.mode = value & 0x1F;
    }
protected:
    virtual uint8_t Read8(uint32_t addr) = 0;
    virtual uint16_t Read16(uint32_t addr) = 0;
    virtual uint32_t Read32(uint32_t addr) = 0;

    virtual void Write8(uint32_t addr, uint8_t data) = 0;
    virtual void Write16(uint32_t addr, uint16_t data) = 0;
    virtual void Write32(uint32_t addr, uint32_t data) = 0;

    void SwitchMode(uint8_t mode);
public:
    void DoInterrupt()
    {
        printf("[ARM%d]: Entering interrupt (0x%08x %d)\n", id, cpsr.t ? *(registers[15]) - 4 : *(registers[15]) - 8, cpsr.t);
        regs_irq[1] = cpsr.t ? (*(registers[15])) : (*(registers[15]) - 4);
        spsr_irq.value = cpsr.value;
        cpsr.t = 0;
        cpsr.i = 1;
        cpsr.f = 1;
        cpsr.mode = MODE_IRQ;
        SwitchMode(MODE_IRQ);
        *(registers[15]) = (id == 9) ? 0xFFFF0018+8 : 0x18+8;
    }

    virtual void Dump()
    {
        for (int i = 0; i < 16; i++)
            printf("r%d\t->\t0x%08x\n", i, *(registers[i]));
        printf("[%s%s%s%s%s%s]\n", 
            cpsr.c ? "c" : ".", 
            cpsr.z ? "z" : ".", 
            cpsr.n ? "n" : ".", 
            cpsr.v ? "v" : ".",
            cpsr.i ? "i" : ".",
            cpsr.t ? "t" : ".");
    }
};

class ARMGeneric
{
private:
    static void BranchExchange(ARMCore* core, uint32_t instr);
    static void BlockDataTransfer(ARMCore* core, uint32_t instr);
    static void Branch(ARMCore* core, uint32_t instr);
    static void SingleDataTransfer(ARMCore* core, uint32_t instr);
    static void BlxReg(ARMCore* core, uint32_t instr);
    static void HalfwordDataTransferReg(ARMCore* core, uint32_t instr);
    static void HalfwordDataTransferImm(ARMCore* core, uint32_t instr);
    static void PsrTransferMRS(ARMCore* core, uint32_t instr);
    static void PsrTransferMSR(ARMCore* core, uint32_t instr);
    static void DataProcessing(ARMCore* core, uint32_t instr);
    static void MoveFromCP(ARMCore* core, uint32_t instr);
    static void MoveToCP(ARMCore* core, uint32_t instr);
    static void BlxOffset(ARMCore* core, uint32_t instr);
    static void Umull(ARMCore* core, uint32_t instr);
    static void Smull(ARMCore* core, uint32_t instr);
    static void Umlal(ARMCore* core, uint32_t instr);
    static void Mla(ARMCore* core, uint32_t instr);
    static void Clz(ARMCore* core, uint32_t instr);
    static void Wfi(ARMCore* core, uint32_t instr);
    static void ChangeStateAndMode(ARMCore* core, uint32_t instr);
	static void Mul(ARMCore* core, uint32_t instr);

    // THUMB mode
    static void PushPop(ARMCore* core, uint16_t instr);
    static void PCRelativeLoad(ARMCore* core, uint16_t instr);
    static void LongBranchFirstHalf(ARMCore* core, uint16_t instr);
    static void LongBranchSecondHalf(ARMCore* core, uint16_t instr);
    static void LongBranchExchange(ARMCore* core, uint16_t instr);
    static void LoadStoreImmOffs(ARMCore* core, uint16_t instr);
    static void LoadStoreRegOffs(ARMCore* core, uint16_t instr);
    static void MovCmpAddSub(ARMCore* core, uint16_t instr);
    static void ConditionalBranch(ARMCore* core, uint16_t instr);
    static void HiRegisterOps(ARMCore* core, uint16_t instr);
    static void DoAddSub(ARMCore* core, uint16_t instr);
    static void DoShift(ARMCore* core, uint16_t instr);
    static void AddSubSP(ARMCore* core, uint16_t instr);
    static void ALUOperations(ARMCore* core, uint16_t instr);
    static void UnconditionalBranch(ARMCore* core, uint16_t instr);
    static void LoadStoreHalfword(ARMCore* core, uint16_t instr);
    static void PCSPOffset(ARMCore* core, uint16_t instr);
    static void SignedUnsignedExtend(ARMCore* core, uint16_t instr);
    static void SPRelativeLoadStore(ARMCore* core, uint16_t instr);
    static void ThumbLDMSTM(ARMCore* core, uint16_t instr);

    static void UpdateFlagsSub(ARMCore* core, uint32_t source, uint32_t operand2, uint32_t result);
    static void UpdateFlagsSbc(ARMCore* core, uint32_t source, uint32_t operand2, uint32_t result);
	static void UpdateFlagsAdd(ARMCore* core, uint32_t a, uint32_t b, uint32_t result);
public:
    static void DoARMInstruction(ARMCore* core, uint32_t instr);
    static void DoTHUMBInstruction(ARMCore* core, uint16_t instr);
};