#pragma once
#include <stdint.h>
enum { STREAM_BUFFER_SIZE=1024 };
static uint8_t stream_bytes[STREAM_BUFFER_SIZE];
static unsigned stream_head,stream_tail;
static uint16_t stream_crc_byte(uint16_t crc,uint8_t value) {
    crc^=(uint16_t)value<<8;
    for (unsigned bit=0;bit<8;++bit)
        crc=(uint16_t)((crc&0x8000U) ? ((uint32_t)crc<<1)^0x1021U : (uint32_t)crc<<1);
    return crc;
}
static void stream_push(uint8_t value) {
    stream_bytes[stream_head]=value;
    stream_head=(stream_head+1)&(STREAM_BUFFER_SIZE-1);
}
// All-or-nothing enqueue. No UART wait occurs in the control loop.
static int stream_frame(unsigned tag,const int *values,unsigned n) {
    unsigned length=n*4;
    unsigned available=(stream_tail-stream_head-1)&(STREAM_BUFFER_SIZE-1);
    if (sizeof(int)!=4 || length>64 || length+6>available) return 0;
    uint16_t crc=0xffffU;
    stream_push(0xa5); stream_push(0x5a);
    stream_push((uint8_t)tag); crc=stream_crc_byte(crc,(uint8_t)tag);
    stream_push((uint8_t)length); crc=stream_crc_byte(crc,(uint8_t)length);
    for (unsigned i=0;i<n;++i) {
        uint32_t value=(uint32_t)values[i];
        for (unsigned j=0;j<4;++j) {
            uint8_t byte=(uint8_t)(value>>(8*j));
            stream_push(byte); crc=stream_crc_byte(crc,byte);
        }
    }
    stream_push((uint8_t)crc); stream_push((uint8_t)(crc>>8));
    return 1;
}
static int stream_pop(uint8_t *value) {
    if (stream_head==stream_tail) return 0;
    *value=stream_bytes[stream_tail];
    stream_tail=(stream_tail+1)&(STREAM_BUFFER_SIZE-1);
    return 1;
}
