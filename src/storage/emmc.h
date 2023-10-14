#pragma once

#include <stdint.h>
#include <string>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <cassert>

namespace eMMC
{

void Initialize(std::string fileName);

uint16_t Read16(uint32_t addr);
void Write16(uint32_t addr, uint16_t data);


uint32_t read_fifo32();

}