#pragma once

#include <stdint.h>

namespace NDMA
{

uint32_t Read32(uint32_t addr);
void Write32(uint32_t addr, uint32_t data);

}