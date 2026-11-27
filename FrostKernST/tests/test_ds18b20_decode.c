#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ds18b20.h"
static unsigned char crc(const unsigned char *p){unsigned char c=0;for(int i=0;i<8;i++){c^=p[i];for(int j=0;j<8;j++)c=(c>>1)^((c&1)?0x8c:0);}return c;}
static void check(int raw,int expected){uint8_t b[9]={(uint8_t)raw,(uint8_t)(raw>>8),75,70,0x7f,0xff,0x0c,0x10,0};b[8]=crc(b);int16_t t=0;assert(DS18B20_DecodeScratchpad(b,&t)==DS18B20_OK&&t==expected);b[8]^=1;assert(DS18B20_DecodeScratchpad(b,&t)==DS18B20_CRC_ERROR);}
int main(void){check(400,2500);check(-84,-525);check(-880,-5500);check(2000,12500);uint8_t b[9]={0};int16_t t;assert(DS18B20_DecodeScratchpad(b,&t)==DS18B20_CRC_ERROR);assert(DS18B20_DecodeScratchpad(b,0)==DS18B20_CRC_ERROR);uint8_t clone[9]={0x90,0x01,75,70,0x7f,0,0,0,0};clone[8]=crc(clone);
assert(DS18B20_DecodeScratchpad(clone,&t)==DS18B20_OK&&t==2500);
puts("PASS DS18B20: real scratchpad CRC, all-zero rejection and compatible reserved bytes, signed conversion and boundary temperatures.");}
