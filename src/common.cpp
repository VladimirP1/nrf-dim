#include "common.h"

void EepWrite16(int pos, uint16_t val) {
    EEPROM.write(pos * 2, (val >> 8) & 0xff);
    EEPROM.write(pos * 2 + 1, val & 0xff);
}

uint16_t EepRead16(int pos) {
    uint16_t ret = 0;
    ret = EEPROM.read(2 * pos);
    ret <<= 8;
    ret |= EEPROM.read(2 * pos + 1);
    return ret;
}