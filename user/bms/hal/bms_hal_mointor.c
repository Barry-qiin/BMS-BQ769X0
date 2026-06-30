



#include "bms_hal_mointor.h"
#include "bms_mointor.h"
#include "bq76920_iic.h"
#include "bms_utils.h"
#include "bms_config.h"
#include "bms_global.h"

#include <string.h>


// 冒泡排序的比较程序,对电压数据进行比较
static int compaer_cell(void *e1, void *e2)
{
	float temp1, temp2;
	
	temp1 = (*(BMS_CellDataTypedef *)e1).CellVoltage;
	temp2 = (*(BMS_CellDataTypedef *)e2).CellVoltage;

	if (temp1 > temp2)
	{
		return 1;
	}

    return 0;
}

void Bms_HalMonitorCellVoltage(void)
{
    uint8_t index = 0;
    
    BQ769X0_UpdateCellVolt();
    for(index = 0; index < BQ769X0_CELL_MAX; index++)
    {
        BMS_MointorData.CellVoltage[index] = BQ769X0_SampleData.CellVoltage[index];
        BMS_MointorData.CellData[index].CellVoltage = BMS_MointorData.CellVoltage[index];
        BMS_MointorData.CellData[index].CellNumber = index;
    }

    BubbleSort(BMS_MointorData.CellData, BQ769X0_CELL_MAX, sizeof(BMS_CellDataTypedef), compaer_cell);
}

void Bms_HalMonitorBatteryVoltage(void)
{  
    BQ769X0_UpadteBatVolt();
    BMS_MointorData.BatteryVoltage = BQ769X0_SampleData.BatteryVoltage;
}

void Bms_HalMonitorBatteryCurrent(void)
{  
    BQ769X0_UpdateCurrent();
    BMS_MointorData.BatteryCurrent = BQ769X0_SampleData.BatteryCurrent;
}

void Bms_HalMonitorCellTemperature(void)
{
    uint8_t index1 = 0,index2 = 0;

    BQ769X0_UpdateTsTemp();
    for(index1 = 0; index1 < BQ769X0_TMEP_MAX; index1++)
    {
        if(BQ769X0_SampleData.Temperature[index1] >= BMS_TEMP_MEASURE_MIN && BQ769X0_SampleData.Temperature[index1] <= BMS_TEMP_MEASURE_MAX)
        {
            BMS_MointorData.CellTempera[index2] = BQ769X0_SampleData.Temperature[index1];
            index2++;
        }
    }
    BMS_MointorData.CellTempEffectiveNumber = index2;

    // 冒泡排序
    BubbleFloat(BMS_MointorData.CellTempera, index2);
}

bool Bms_HalMonitorLoadDetect(void)
{
	// BQ芯片只有在未开启充电的情况下并且CHG引脚电压大于0.7V才能够检测到负载
	return BQ769X0_LoadDetect();
}

