#include "Bus.h"
#include <crypto/rsa.h>
#include <crypto/sha.h>
#include <crypto/aes.h>
#include <dma/cdma.h>
#include <dma/ndma.h>
#include <i2c/i2c.h>
#include <pxi/pxi.h>
#include <timers/arm9_timers.h>
#include <string.h>
#include <storage/emmc.h>
#include <gpu/gpu.h>

uint8_t* bios9, *bios11, *boot9, *boot11;
uint8_t* bios9_locked, *bios11_locked;
uint8_t* axi_wram;
CDMA* dma11, *dma9;
PicaGpu* gpu;

uint8_t itcm[0x8000], dtcm[0x4000];
uint8_t arm9_wram[0x100000];
uint32_t itcm_start, itcm_size;
uint32_t dtcm_start, dtcm_size;

uint8_t *otp, otp_locked[256], otp_free[256];

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
    boot9 = bios9;
    bios9_locked = new uint8_t[size];
    memcpy(bios9_locked, bios9, 0x8000);
    memset(bios9_locked+0x8000, 0, size-0x8000);


    file.open(bios11Path, std::ios::ate | std::ios::binary);
    size = file.tellg();
    file.seekg(0, std::ios::beg);
    bios11 = new uint8_t[size];
    file.read((char*)bios11, size);
    file.close();
    boot11 = bios11;
    bios11_locked = new uint8_t[size];
    memcpy(bios11_locked, bios11, 0x8000);
    memset(bios11_locked+0x8000, 0, size-0x8000);

    axi_wram = new uint8_t[0x80000];
    dma11 = new CDMA(std::bind(&ARM11::Write32, std::placeholders::_1, std::placeholders::_2), 
                        std::bind(&ARM11::Read32, std::placeholders::_1),
                        std::bind(&ARM11::Read8, std::placeholders::_1));
    dma9 = new CDMA(std::bind(&ARM9::Write32, std::placeholders::_1, std::placeholders::_2), 
                        std::bind(&ARM9::Read32, std::placeholders::_1),
                        std::bind(&ARM9::Read8, std::placeholders::_1));
    
    if (isnew)
        socinfo = 7;
    else
        socinfo = 1;
    
    memset(otp_locked, 0xFF, sizeof(otp_locked));

    std::fstream nand_file("nand.bin", std::ios::binary | std::ios::in | std::ios::out);

    char essentials[0x200];
    nand_file.seekg(0x200);
    nand_file.read((char*)essentials, 0x200);

    bool otp_found = false;

    otp = otp_free;

    int counter = 0;
    while (!otp_found && counter < 0x200)
    {
        uint32_t offs = *(uint32_t*)&essentials[counter + 8];

        offs += 0x400;
        if (!strncmp(essentials+counter, "otp", 8))
        {
            nand_file.seekg(offs);
            nand_file.read((char*)otp, 256);
            otp_found = true;
        }
        counter += 0x10;
    }

    if (!otp_found)
    {
        printf("ERROR: bad nand.bin, no OTP found!\n");
        exit(1);
    }

    eMMC::Initialize("nand.bin");

    gpu = new PicaGpu();
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

    gpu->Dump();
}

void Bus::Reset()
{
    AES::Reset();
    gpu->Reset();
}

void Bus::Run()
{
    for (int i = 0; i < 2; i++)
        dma11->Tick();
    dma9->Tick();
    Timers::Tick();
}

bool Bus::GetInterruptPending9()
{
    return irq_ie & irq_if;
}

void Bus::SetInterruptPending9(uint32_t interrupt)
{
    irq_if |= (1 << interrupt);
}

uint8_t Bus::ARM11::Read8(uint32_t addr)
{
    if (addr < 0x20000)
        return boot11[addr & 0xFFFF];
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
        return *(uint16_t*)&boot11[addr & 0xFFFF];
    if (addr > 0x1FF80000 && addr < 0x20000000)
        return *(uint16_t*)&axi_wram[addr & 0x7FFFF];
    
    switch (addr)
    {
    case 0x10146000:
        return 0xFFF;
    case 0x10140FFC:
        return socinfo;
    case 0x10163004:
        return PXI::ReadCnt11();
    }

    printf("Read16 from unknown addr 0x%08x\n", addr);
    exit(1);
}

uint32_t Bus::ARM11::Read32(uint32_t addr)
{
    if (addr < 0x20000)
        return *(uint32_t*)&boot11[addr & 0xFFFF];
    if (addr > 0x1FF80000 && addr < 0x20000000)
        return *(uint32_t*)&axi_wram[addr & 0x7FFFF];
    if (addr >= 0x18000000 && addr < 0x18C00000)
        return gpu->Read32(addr);
    
    switch (addr)
    {
    case 0x10141200:
    case 0x10400030:
    case 0x10400004:
        return 0;
    case 0x10200020:
    case 0x1020002C:
    case 0x10200100:
    case 0x10200d00:
        return dma11->Read32(addr);
    case 0x10163000:
        return PXI::ReadSync11();
    case 0x1016300C:
        return PXI::ReadRecv11();
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
    if (addr >= 0x18000000 && addr < 0x18C00000)
        return gpu->Write32(addr, data);

    switch (addr)
    {
    case 0x10141200:
    case 0x10400030:
    case 0x10202014:
    case 0x10400004:
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
    if (addr >= dtcm_start && addr < dtcm_start+dtcm_size)
        return dtcm[addr & 0x3FFF];
    if (addr >= itcm_start && addr < itcm_start+itcm_size)
        return itcm[addr & 0x7FFF];
    if (addr >= 0xFFFF0000)
        return boot9[addr & 0xFFFF];
    if (addr >= 0x08000000 && addr < 0x08100000)
        return arm9_wram[addr & 0xFFFFF];
    if (addr >= 0x1000A040 && addr < 0x1000A080)
        return SHA::ReadHash(addr);
    if (addr >= 0x1000B000 && addr < 0x1000C000)
        return RSA::Read8(addr);
    if (addr >= 0x10160000 && addr < 0x10161000)
        return 0;
    
    switch (addr)
    {
    case 0x10000000:
    case 0x10000001:
        return 0x1;
    case 0x10000002:
    case 0x10010010:
    case 0x10000008:
        return 0;
    case 0x10009011:
        return AES::ReadKEYCNT();
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
        return *(uint16_t*)&boot9[addr & 0xFFFF];
    if (addr >= 0x10006000 && addr < 0x10007000)
        return eMMC::Read16(addr);
    
    switch (addr)
    {
    case 0x10003000 ... 0x1000300E:
        return Timers::Read16(addr);
    case 0x10146000:
        return 0xFFF;
    case 0x10008004:
        return PXI::ReadCnt9();
    }

    printf("[ARM9]: Read16 unknown addr 0x%08x\n", addr);
    exit(1);
}

uint32_t Bus::ARM9::Read32(uint32_t addr)
{
    if (addr >= 0xFFFF0000)
        return *(uint32_t*)&boot9[addr & 0xFFFF];
    if (addr >= itcm_start && addr < itcm_start+itcm_size)
        return *(uint32_t*)&itcm[addr & 0x7FFF];
    if (addr >= dtcm_start && addr < dtcm_start+dtcm_size)
        return *(uint32_t*)&dtcm[addr & 0x3FFF];
    if (addr >= 0x1FF80000 && addr < 0x20000000)
        return *(uint32_t*)&axi_wram[addr & 0x7FFFF];
    if (addr >= 0x08000000 && addr < 0x08100000)
        return *(uint32_t*)&arm9_wram[addr & 0xFFFFF];
    if (addr >= 0x10002000 && addr < 0x10003000)
        return NDMA::Read32(addr);
    if (addr >= 0x10009000 && addr < 0x1000A000)
        return AES::Read32(addr);
    if (addr >= 0x1000A000 && addr < 0x1000B000)
        return SHA::Read32(addr);
    if (addr >= 0x1000B000 && addr < 0x1000C000)
        return RSA::Read32(addr);
    if (addr >= 0x10012000 && addr < 0x10012100)
        return *(uint32_t*)&otp[addr & 0xFF];
    
    switch (addr)
    {
    case 0x10001000: return irq_ie;
    case 0x10001004: return irq_if;
    case 0x1000201C:
    case 0x1000610c:
        return eMMC::read_fifo32();
    case 0x1000C020:
    case 0x1000C02C:
    case 0x1000c100:
    case 0x1000cd04:
    case 0x1000cd08:
    case 0x1000cd0c:
    case 0x1000cd00:
        return dma9->Read32(addr);
    case 0x10008000:
        return PXI::ReadSync9();
    }

    printf("[ARM9]: Read32 unknown addr 0x%08x\n", addr);
    exit(1);
}

void Bus::ARM9::Write8(uint32_t addr, uint8_t data)
{
    if (addr >= itcm_start && addr < itcm_start+itcm_size)
    {
        itcm[addr & 0x7FFF] = data;
        return;
    }
    if (addr >= dtcm_start && addr < dtcm_start+dtcm_size)
    {
        dtcm[addr & 0x3FFF] = data;
        return;
    }
    if (addr >= 0x08000000 && addr < 0x08100000)
    {
        arm9_wram[addr & 0xFFFFF] = data;
        return;
    }
    if (addr >= 0x1000B000 && addr < 0x1000C000)
        return RSA::Write8(addr, data);
    if (addr >= 0x1ff80000 && addr < 0x20000000)
    {
        axi_wram[addr & 0x7FFFF] = data;
        return;
    }
    if (addr >= 0x10160000 && addr < 0x10161000)
        return;

    switch (addr)
    {
    case 0x10000000:
    {
        if (data & 1)
            boot9 = bios9_locked;
        if (data & 2)
            otp = otp_locked;
        return;
    }
    case 0x10000001:
        if (data & 1)
            boot11 = bios11_locked;
        return;
    case 0x10000002:
    case 0x10000008:
        return;
    case 0x10009010:
        AES::WriteKEYSEL(data);
        return;
    case 0x10009011:
        AES::WriteKEYCNT(data);
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
    if (addr >= 0x10006000 && addr < 0x10007000)
        return eMMC::Write16(addr, data);
    if (addr >= 0x10160000 && addr < 0x10161000)
        return;

    switch (addr)
    {
    case 0x10003000 ... 0x1000300E:
        return Timers::Write16(addr, data);
    case 0x10008004:
        return PXI::WriteCnt9(data);
    case 0x10009006:
        AES::WriteBlockCount(data);
        return;
    }

    printf("Write16 unknown addr 0x%08x\n", addr);
    exit(1);
}

void Bus::ARM9::Write32(uint32_t addr, uint32_t data)
{
    if (addr >= itcm_start && addr < itcm_start+itcm_size)
    {
        *(uint32_t*)&itcm[addr & 0x7FFF] = data;
        return;
    }
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
    if (addr >= 0x1ff80000 && addr < 0x20000000)
    {
        *(uint32_t*)&axi_wram[addr & 0x7FFFF] = data;
        return;
    }

    if (addr >= 0x10002000 && addr < 0x10003000)
        return NDMA::Write32(addr, data);
    if (addr >= 0x10009000 && addr < 0x1000A000)
        return AES::Write32(addr, data);
    if (addr >= 0x1000A000 && addr < 0x1000B000)
        return SHA::Write32(addr, data);
    if ((addr & 0xF0000000) == 0xC0000000)
        return;

    switch (addr)
    {
    case 0x10001000:
        irq_ie = data;
        printf("[IRQ9]: Setting IE to 0x%08x\n", data);
        return;
    case 0x10001004:
        irq_if &= ~data;
        printf("[IRQ9]: Write 0x%08x to IF\n", data);
        return;
    case 0x1000C020:
    case 0x1000C02C:
    case 0x1000cd04:
    case 0x1000cd08:
    case 0x1000cd0c:
        return dma9->Write32(addr, data);
    case 0x10008000:
        return PXI::WriteSync9(data);
    case 0x10008008:
        return PXI::WriteSend9(data);
    case 0x1000B000 ... 0x1000B900:
        return RSA::Write32(addr, data);
    }

    printf("[ARM9]: Write32 unknown addr 0x%08x\n", addr);
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
