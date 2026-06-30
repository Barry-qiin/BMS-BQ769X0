

#ifndef __BMS_UTILS_H__
#define __BMS_UTILS_H__

#include <stdint.h>


void swap(uint8_t *buf1, uint8_t *buf2, uint32_t width);
void BubbleFloat(float a[], uint32_t n);
void BubbleSort(void *base, uint32_t sz, uint32_t width, int (*cmp)(void *e1, void *e2));
int right_bound(uint16_t *nums,uint16_t start_pos,uint16_t end_pos,uint16_t target );


#endif


