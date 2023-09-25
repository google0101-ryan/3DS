#include "Bus.h"

uint8_t* bios9, *bios11;

void Bus::Initialize(std::string bios9Path, std::string bios11Path)
{
    std::ifstream file(bios9Path, std::ios::ate | std::ios::binary);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    bios9 = new uint8_t[size];
    file.read((char*)bios9, size);
    file.close();

    file.open(bios11Path, std::ios::ate | std::ios::binary);
    size = file.tellg();
    file.seekg(0, std::ios::beg);
    bios11 = new uint8_t[size];
    file.read((char*)bios11, size);
    file.close();
}

uint32_t Bus::ARM11::Read32(uint32_t addr)
{
    if (addr < 0x20000)
        return *(uint32_t*)&bios11[addr & 0xFFFF];

    printf("Read32 from unknown addr 0x%08x\n", addr);
    exit(1);
}
