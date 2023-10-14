#include "gpu.h"

#include <fstream>

void PicaGpu::Reset()
{
    vram_a = new uint8_t[3*1024*1024];
    vram_b = new uint8_t[3*1024*1024];
    vram_a_base = 0x18000000;
    vram_b_base = 0x18300000;
}

void PicaGpu::Dump()
{
    std::ofstream file_a("vram_a.bin"), file_b("vram_b.bin");
    for (int i = 0; i < 3*1024*1024; i++)
    {
        file_a << vram_a[i];
        file_b << vram_b[i];
    }

    file_a.close();
    file_b.close();
}

uint32_t PicaGpu::Read32(uint32_t addr)
{
    if (addr >= vram_a_base && addr < vram_a_base+0x300000)
        return *(uint32_t*)&vram_a[addr & 0x2FFFFF];
    if (addr >= vram_b_base && addr < vram_b_base+0x300000)
        return *(uint32_t*)&vram_b[addr & 0x2FFFFF];
    
    printf("[PICA]: Read from unknown addr 0x%08x\n", addr);
    exit(1);
}

void PicaGpu::Write32(uint32_t addr, uint32_t data)
{
    if (addr >= vram_a_base && addr < vram_a_base+0x300000)
    {
        *(uint32_t*)&vram_a[addr & 0x2FFFFF] = data;
        return;
    }
    if (addr >= vram_b_base && addr < vram_b_base+0x300000)
    {
        *(uint32_t*)&vram_b[addr & 0x2FFFFF] = data;
        return;
    }
    
    printf("[PICA]: Write to unknown addr 0x%08x\n", addr);
    exit(1);
}
