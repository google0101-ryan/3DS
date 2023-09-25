#pragma once

#include <stdint.h>
#include <arm/armgeneric.h>

class ARM11Core : public ARMCore
{
private:
    int coreId = 0;
public:
    ARM11Core();

    void Reset();
    void Run();
    void Dump();

    virtual uint32_t Read32(uint32_t addr);
};