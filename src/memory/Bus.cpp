#include "Bus.h"
#include <dma/cdma.h>
#include <i2c/i2c.h>
#include <pxi/pxi.h>
#include <timers/arm9_timers.h>

uint8_t* bios9, *bios11;
uint8_t* axi_wram;
CDMA* dma11;

uint8_t itcm[0x8000], dtcm[0x4000];
uint8_t arm9_wram[0x100000];
uint32_t itcm_start, itcm_size;
uint32_t dtcm_start, dtcm_size;

uint16_t socinfo;

uint32_t irq_ie = 0, irq_if = 0;

void Bus::Initialize(std::string bios9Path, std::string bios11Path, bool isnew)
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

    axi_wram = new uint8_t[0x80000];
    dma11 = new CDMA(std::bind(&ARM11::Write32, std::placeholders::_1, std::placeholders::_2), 
                        std::bind(&ARM11::Read32, std::placeholders::_1),
                        std::bind(&ARM11::Read8, std::placeholders::_1));
    
    if (isnew)
        socinfo = 7;
    else
        socinfo = 1;
}

void Bus::Dump()
{
    std::ofstream outfile("axi_wram.bin");
    outfile.write((char*)axi_wram, 0x80000);
    outfile.close();

    outfile.open("dtcm.bin");
    outfile.write((char*)dtcm, sizeof(dtcm));
    outfile.close();

    outfile.open("itcm.bin");
    outfile.write((char*)itcm, sizeof(itcm));
    outfile.close();

    outfile.open("arm9_ram.bin");
    outfile.write((char*)arm9_wram, sizeof(arm9_wram));
    outfile.close();
}

void Bus::Run()
{
    dma11->Tick();
}

uint8_t Bus::ARM11::Read8(uint32_t addr)
{
    if (addr < 0x20000)
        return bios11[addr & 0xFFFF];
    if (addr > 0x1FF80000 && addr < 0x20000000)
        return axi_wram[addr & 0x7FFFF];
    
    switch (addr)
    {
    case 0x10141204:
    case 0x10141208:
    case 0x10147000:
        return 0;
    case 0x10141220:
        return 2;
    case 0x10161001:
    case 0x10144001:
    case 0x10148001:
        return I2C::Read8(addr);
    }

    printf("Read8 from unknown addr 0x%08x\n", addr);
    exit(1);
}

uint16_t Bus::ARM11::Read16(uint32_t addr)
{
    if (addr < 0x20000)
        return *(uint16_t*)&bios11[addr & 0xFFFF];
    if (addr > 0x1FF80000 && addr < 0x20000000)
        return *(uint16_t*)&axi_wram[addr & 0x7FFFF];
    
    switch (addr)
    {
    case 0x10146000:
        return 0xFFF;
    case 0x10140FFC:
        return socinfo;
    }

    printf("Read16 from unknown addr 0x%08x\n", addr);
    exit(1);
}

uint32_t Bus::ARM11::Read32(uint32_t addr)
{
    if (addr < 0x20000)
        return *(uint32_t*)&bios11[addr & 0xFFFF];
    if (addr > 0x1FF80000 && addr < 0x20000000)
        return *(uint32_t*)&axi_wram[addr & 0x7FFFF];
    
    switch (addr)
    {
    case 0x10141200:
    case 0x10400030:
        return 0;
    case 0x10200020:
    case 0x1020002C:
    case 0x10200100:
    case 0x10200d00:
        return dma11->Read32(addr);
    case 0x10163000:
        return PXI::ReadSync11();
    }

    printf("Read32 from unknown addr 0x%08x\n", addr);
    exit(1);
}

void Bus::ARM11::Write8(uint32_t addr, uint8_t data)
{
    if (addr > 0x1FF80000 && addr < 0x20000000)
    {
        axi_wram[addr & 0x7FFFF] = data;
        return;
    }
    else if (addr >= 0x10140000 && addr < 0x10140010)
        return;
    
    switch (addr)
    {
    case 0x10141204:
    case 0x10141208:
    case 0x10141220:
        return;
    case 0x10144000:
    case 0x10161000:
    case 0x10148000:
    case 0x10161001:
    case 0x10144001:
    case 0x10148001:
        I2C::Write8(addr, data);
        return;
    }

    printf("Write8 0x%02x to unknown addr 0x%08x\n", data, addr);
    exit(1);
}

void Bus::ARM11::Write16(uint32_t addr, uint16_t data)
{
    if (addr > 0x1FF80000 && addr < 0x20000000)
    {
        *(uint16_t*)&axi_wram[addr & 0x7FFFF] = data;
        return;
    }

    switch (addr)
    {
    case 0x10161002:
    case 0x10161004:
    case 0x10144002:
    case 0x10144004:
    case 0x10148002:
    case 0x10148004:
        I2C::Write16(addr, data);
        return;
    case 0x10163004:
        PXI::WriteCnt11(data);
        return;
    }

    printf("Write16 unknown addr 0x%08x\n", addr);
    exit(1);
}

void Bus::ARM11::Write32(uint32_t addr, uint32_t data)
{
    if (addr > 0x1FF80000 && addr < 0x20000000)
    {
        *(uint32_t*)&axi_wram[addr & 0x7FFFF] = data;
        return;
    }

    switch (addr)
    {
    case 0x10141200:
    case 0x10400030:
    case 0x10202014:
        return;
    case 0x10200020:
    case 0x1020002C:
    case 0x10200d04:
    case 0x10200d08:
    case 0x10200d0c:
        return dma11->Write32(addr, data);
    case 0x10163000:
        return PXI::WriteSync11(data);
    }

    printf("Write32 unknown addr 0x%08x\n", addr);
    exit(1);
}

uint8_t Bus::ARM9::Read8(uint32_t addr)
{
    switch (addr)
    {
    case 0x10000002:
        return 0;
    }

    printf("Read8 unknown addr 0x%08x\n", addr);
    exit(1);
}

uint16_t Bus::ARM9::Read16(uint32_t addr)
{
    if (addr >= dtcm_start && addr < dtcm_start+dtcm_size)
    {
        return *(uint16_t*)&dtcm[addr & 0x3FFF];
    }
    if (addr >= 0xFFFF0000)
        return *(uint16_t*)&bios9[addr & 0xFFFF];

    printf("Read16 unknown addr 0x%08x\n", addr);
    exit(1);
}

uint32_t Bus::ARM9::Read32(uint32_t addr)
{
    if (addr >= 0xFFFF0000)
        return *(uint32_t*)&bios9[addr & 0xFFFF];
    if (addr >= dtcm_start && addr < dtcm_start+dtcm_size)
        return *(uint32_t*)&dtcm[addr & 0x3FFF];
    
    switch (addr)
    {
    case 0x10001000: return irq_ie;
    case 0x10001004: return irq_if;
    case 0x1000201C:
        return 0;
    }

    printf("Read32 unknown addr 0x%08x\n", addr);
    exit(1);
}

void Bus::ARM9::Write8(uint32_t addr, uint8_t data)
{
    if (addr >= dtcm_start && addr < dtcm_start+dtcm_size)
    {
        dtcm[addr & 0x3FFF] = data;
        return;
    }

    switch (addr)
    {
    case 0x10000002:
        return;
    }

    printf("Write8 unknown addr 0x%08x\n", addr);
    exit(1);
}

void Bus::ARM9::Write16(uint32_t addr, uint16_t data)
{
    if (addr >= dtcm_start && addr < dtcm_start+dtcm_size)
    {
        *(uint16_t*)&dtcm[addr & 0x3FFF] = data;
        return;
    }

    switch (addr)
    {
    case 0x10003000 ... 0x1000300E:
        return Timers::Write16(addr, data);
    }

    printf("Write16 unknown addr 0x%08x\n", addr);
    exit(1);
}

void Bus::ARM9::Write32(uint32_t addr, uint32_t data)
{
    if (addr >= dtcm_start && addr < dtcm_start+dtcm_size)
    {
        *(uint32_t*)&dtcm[addr & 0x3FFF] = data;
        return;
    }
    if (addr >= 0x08000000 && addr < 0x08100000)
    {
        *(uint32_t*)&arm9_wram[addr & 0xFFFFF] = data;
        return;
    }

    switch (addr)
    {
    case 0x10001000:
        irq_ie = data;
        printf("[IRQ9]: Setting IE to 0x%08x\n", data);
        return;
    case 0x10001004:
        irq_if &= ~data;
        return;
    }

    printf("Write32 unknown addr 0x%08x\n", addr);
    exit(1);
}

void Bus::ARM9::RemapTCM(uint32_t addr, uint32_t size, bool itcm)
{
    if (itcm)
    {
        itcm_start = addr;
        itcm_size = size;
        printf("Remapping ITCM to 0x%08x, 0x%08x bytes\n", addr, size);
    }
    else
    {
        dtcm_start = addr;
        dtcm_size = size;
        printf("Remapping DTCM to 0x%08x, 0x%08x bytes\n", addr, size);
    }
}
