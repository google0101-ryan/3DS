#pragma once

#include <stdint.h>

class CP15
{
private:
    int coreId;

    bool isMMUEnabled = false;
    bool highExceptionVectors = false;

    uint32_t aux_control = 0xF;
public:
    int procID;

    void SetCoreID(int id) {coreId = id;}

    void WriteRegister(int cpopc, int cn, int cm, int cp, uint32_t data);
    uint32_t ReadRegister(int cpopc, int cn, int cm, int cp);
};