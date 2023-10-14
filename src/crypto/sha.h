#pragma once

#include <stdint.h>

namespace SHA
{

void Write32(uint32_t addr, uint32_t data);
uint32_t Read32(uint32_t addr);

uint8_t ReadHash(uint32_t addr);

}