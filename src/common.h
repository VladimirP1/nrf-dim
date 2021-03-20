#pragma once
#include <EEPROM.h>

void EepWrite16(int pos, uint16_t val);

uint16_t EepRead16(int pos);