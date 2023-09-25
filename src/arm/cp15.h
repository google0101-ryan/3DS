#pragma once

#include <stdint.h>

class CP15
{
public:
    void WriteRegister(int cpopc, int cn, int cm, int cp);
    uint32_t ReadRegister(int cpopc, int cn, int cm, int cp);
};