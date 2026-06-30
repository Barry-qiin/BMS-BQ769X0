

#ifndef __BMS_MOINTOR_H__
#define __BMS_MOINTOR_H__ 

#include "bq76920_iic.h"


typedef struct  
{
  	float CellVoltage; 		// 电芯电压(V)
	uint32_t CellNumber;	// 电芯的编号  
}BMS_CellDataTypedef;

typedef struct 
{
  float CellVoltage[BQ769X0_CELL_MAX];     //电芯电压未排序的
  float BatteryVoltage;
  float BatteryCurrent;
  float CellTempera[BQ769X0_TMEP_MAX];          //通过冒泡排序函数，把数据从小到大排序
  BMS_CellDataTypedef CellData[BQ769X0_CELL_MAX];   //电芯电压数据会从小到大排序的
  uint32_t CellTempEffectiveNumber;				// 有效值的温度数量,为什么需要这个：有的测温超过最高值或者最低值视为无效
}BMS_MointorDataTypedef;

void BMS_MointorInit(void);

void BMS_Mointor_HWCurrent(void);



extern BMS_MointorDataTypedef BMS_MointorData;

















void BMS_MointorInit(void);








#endif



