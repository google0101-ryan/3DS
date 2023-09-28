#pragma once

#include <string>
#include <stdint.h>
#include <fstream>

namespace Bus
{

void Initialize(std::string bios9Path, std::string bios11Path, bool isnew = false);
void Dump();

void Run();

namespace ARM11
{

uint8_t Read8(uint32_t addr);
uint16_t Read16(uint32_t addr);
uint32_t Read32(uint32_t addr);

void Write8(uint32_t addr, uint8_t data);
void Write16(uint32_t addr, uint16_t data);
void Write32(uint32_t addr, uint32_t data);

}

namespace ARM9
{

uint8_t Read8(uint32_t addr);
uint16_t Read16(uint32_t addr);
uint32_t Read32(uint32_t addr);

void Write8(uint32_t addr, uint8_t data);
void Write16(uint32_t addr, uint16_t data);
void Write32(uint32_t addr, uint32_t data);

void RemapTCM(uint32_t addr, uint32_t size, bool itcm);

}

}