#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <bit>

#include "cp15.h"

enum ArmModes
{
    MODE_SUPERVISOR = 0x13
};

union CPSR
{
    uint32_t value;
    struct
    {
        uint32_t n : 1;
        uint32_t z : 1;
        uint32_t c : 1;
        uint32_t v : 1;
        uint32_t q : 1;
        uint32_t rsv0 : 2;
        uint32_t j : 1;
        uint32_t rsv1 : 14;
        uint32_t e : 1;
        uint32_t a : 1;
        uint32_t i : 1;
        uint32_t f : 1;
        uint32_t t : 1;
        uint32_t mode : 5;
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

    CPSR cpsr, *cur_spsr, spsr_svc;

    CP15* cp15 = nullptr;

    bool CanDisassemble = true;
    bool didBranch = false;

    int id; // Either 11, 9, or 7 depending on the ARM core

    uint32_t pipeline[2];
    uint16_t t_pipeline[2];
protected:
    void FillPipeline();
    uint32_t AdvancePipeline();

    virtual uint32_t Read32(uint32_t addr) = 0;

    void SwitchMode(uint8_t mode);
public:
    virtual void Dump()
    {
        for (int i = 0; i < 16; i++)
            printf("r%d\t->\t0x%08x\n", i, *(registers[i]));
    }
};

class ARMGeneric
{
private:
    static void Branch(ARMCore* core, uint32_t instr);
    static void SingleDataTransfer(ARMCore* core, uint32_t instr);
    static void DataProcessing(ARMCore* core, uint32_t instr);
    static void MoveFromCP(ARMCore* core, uint32_t instr);
public:
    static void DoARMInstruction(ARMCore* core, uint32_t instr);
};