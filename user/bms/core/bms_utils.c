


#include "bms_utils.h"

// 交换元素,任意类型
void swap(uint8_t *buf1, uint8_t *buf2, uint32_t width)
{
	uint8_t temp;
    uint32_t i;
    
    for (i = 0; i < width; i++)
    {
        temp = *buf1;
        *buf1 = *buf2;
        *buf2 = temp;
        buf1++;
        buf2++;
    }
}

// 冒泡排序float类型
void BubbleFloat(float a[], uint32_t n)
{
	float t;
    uint32_t i, j;
      
    for (i = 1; i < n; i++)
    {
        for (j = 0; j < n-i; j++)
        {
            if (a[j] > a[j+1])
            {
                t = a[j];
                a[j] = a[j+1];
                a[j+1] = t;
            }
        }
    }
}


// 冒泡排序,任意类型
// base：	基地址
// sz:		要排序元素个数
// width:	单个元素的宽度
// cmp:		不明确类型的情况下,两个数据的对比结果必须由用户完成
//			如果e1比e2大则cmp应返回大于0的数,反之则返回小于等于0的数
void BubbleSort(void *base, uint32_t sz, uint32_t width, int (*cmp)(void *e1, void *e2))
{
    uint32_t i = 0, j = 0;

    for (i = 1; i < sz; i++)
    {
        for (j = 0; j < sz - i; j++)
        {
            if (cmp((uint8_t *)base + j * width, (uint8_t *)base + (j + 1) * width) > 0)
            {
                swap((uint8_t *)base + j * width, (uint8_t *)base + (j + 1) * width, width);
            }
        }
    }
}


// 查找一个数在数组中的右侧边界(二分法)
// start_pos：起始位置
// end_pos：结束位置
// 返回-1：表示不存在这个数
int right_bound(uint16_t *nums,uint16_t start_pos,uint16_t end_pos,uint16_t target )
{
    uint16_t left = start_pos, right = end_pos;
    while(left < right)
    {
        int mid = (left + right) / 2;
        if(nums[mid] < target)
        {
            left = mid + 1;

        }
        else if(nums[mid] >= target)
        {

           right = mid; 
        }
    }

    if(right > end_pos)
    {
        return -1;
    }

    return right;
}




