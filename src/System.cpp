#include "System.h"
#include <memory/Bus.h>
#include <arm/arm11.h>
#include <arm/arm9.h>

ARM11Core cores[4];
ARM9Core arm9;

void System::LoadBios(const char *bios9, const char *bios11)
{
    Bus::Initialize(bios9, bios11);
}

void System::Reset()
{
    for (int i = 0; i < 2; i++)
        cores[i].Reset();
    arm9.Reset();
    Bus::Reset();
}

int System::Run()
{
    while (1)
    {
        for (int i = 0; i < 2; i++)
        {
            cores[0].Run();
            cores[1].Run();
        }

        arm9.Run();

        Bus::Run();
    }

    return 0;
}

void System::Dump()
{
    cores[0].Dump();
    cores[1].Dump();

    arm9.Dump();

    Bus::Dump();
}
