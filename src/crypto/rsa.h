#pragma once

#include <stdint.h>

namespace RSA
{

uint8_t Read8(uint32_t addr);
uint32_t Read32(uint32_t addr);

void Write8(uint32_t addr, uint8_t data);
void Write32(uint32_t addr, uint32_t data);

}