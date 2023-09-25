#pragma once

#include <string>
#include <stdint.h>
#include <fstream>

namespace Bus
{

void Initialize(std::string bios9Path, std::string bios11Path);

namespace ARM11
{

uint32_t Read32(uint32_t addr);

}

}