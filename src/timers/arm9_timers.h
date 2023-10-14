#pragma once

#include <stdint.h>

namespace Timers
{

void Tick();

void Write16(uint32_t addr, uint16_t data);
uint16_t Read16(uint32_t addr);

}