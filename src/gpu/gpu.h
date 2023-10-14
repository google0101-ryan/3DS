#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

class PicaGpu
{
private:
    uint8_t* vram_a, *vram_b;
    uint32_t vram_a_base, vram_b_base;
public:
    void Reset();
    void Dump();

    uint32_t Read32(uint32_t addr);
    void Write32(uint32_t addr, uint32_t data);
};