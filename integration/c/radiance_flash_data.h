#ifndef RADIANCE_FLASH_DATA_H
#define RADIANCE_FLASH_DATA_H
enum { RADIANCE_FLASH_SLOT=93, RADIANCE_FLASH_DONOR=240, RADIANCE_FLASH_FRAMES=53,
       RADIANCE_FLASH_COUNTER_END=34, RADIANCE_FLASH_HIT_BEGIN=8, RADIANCE_FLASH_HIT_END=23 };
static inline float radiance_flash_sample(float frame)
{
    if(frame<8)return 0+((frame-0)*15)/8;
    if(frame<19)return 15+((frame-8)*12)/11;
    if(frame<29)return 27+((frame-19)*9)/10;
    return 36;
}
#endif
