#pragma once
#include <stdint.h>

struct __attribute__ ((packed)) OtaPacket {
    uint16_t src_uuid {};
    uint16_t light_id {};
    uint16_t light_brightness {};
};