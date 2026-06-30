



#ifndef __BMS_GLOBAL_H__
#define __BMS_GLOBAL_H__

#include "bms_config.h"


typedef enum
{
	BMS_SYS_MODE_NULLL,
	BMS_SYS_MODE_CHG,   // 充电
	BMS_SYS_MODE_DISCHG,  // 放电
    BMS_SYS_MODE_STANDBY,  // 待机
    BMS_SYS_MODE_SLEEP,  // 休眠

}BMS_SysModeTypeDef;

typedef enum
{
	BMS_STATE_ENBLE,
	BMS_STATE_DISABLE,

}BMS_StateTypeDef;

typedef enum
{
	BMS_CELL_NULL    = 0x0000,
    BMS_CELL_INDEX1  = 0x0001,
	BMS_CELL_INDEX2  = 0x0002,
	BMS_CELL_INDEX3  = 0x0004,
	BMS_CELL_INDEX4  = 0x0008,
	BMS_CELL_INDEX5  = 0x0010,
	BMS_CELL_INDEX6  = 0x0020,
	BMS_CELL_INDEX7  = 0x0040,
	BMS_CELL_INDEX8  = 0x0080,
	BMS_CELL_INDEX9  = 0x0100,
	BMS_CELL_INDEX10 = 0x0200,
	BMS_CELL_INDEX11 = 0x0400,
	BMS_CELL_INDEX12 = 0x0800,
	BMS_CELL_INDEX13 = 0x1000,
	BMS_CELL_INDEX14 = 0x2000,
	BMS_CELL_INDEX15 = 0x4000,
	BMS_CELL_ALL	 = 0x7FFF,

}BMS_CellIndexTypedef;

/*这个结构体相当于用于跟用户交互的系统默认模式，不论芯片做了什么比如保护等，解除后回到这里设定的系统默认值*/
typedef struct
{
	BMS_SysModeTypeDef SysMode;		// 当前系统模式
    BMS_StateTypeDef Charge;        // 充电状态
    BMS_StateTypeDef Discharge;     // 放电状态
    BMS_StateTypeDef Balance;       // 均衡状态

    uint8_t CellRealNumber;         // 实际电芯数
    uint8_t TempRealNumber;           // 实际温度数
}BMS_GlobalParamTypedef;


extern BMS_GlobalParamTypedef BMS_GlobalParam;

#endif


