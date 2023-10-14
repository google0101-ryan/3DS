#pragma once

#include <stdint.h>
#include <arm/armgeneric.h>
#include <arm/mpcore_pmr.h>

class ARM11Core : public ARMCore
{
private:
    int coreId = 0;
public:
    MPCore_PMR* pmr;
    ARM11Core();

    void Reset();
    void Run();
    void Dump();

    virtual uint8_t Read8(uint32_t addr);
    virtual uint16_t Read16(uint32_t addr);
    virtual uint32_t Read32(uint32_t addr);

    virtual void Write8(uint32_t addr, uint8_t data);
    virtual void Write16(uint32_t addr, uint16_t data);
    virtual void Write32(uint32_t addr, uint32_t data);
};