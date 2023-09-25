#pragma once

namespace System
{

void LoadBios(const char* bios9, const char* bios11);
void Reset();

int Run();
void Dump();

}