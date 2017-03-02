#ifndef crc16_h
#define crc16_h

#include <stdint.h>

uint16_t crc16_update(uint16_t crc, uint8_t data);

uint16_t crc16_block(uint16_t crc, uint8_t *data, int len);

#endif /* crc16_h */
