



#ifndef __BMS_ENERGY_H__
#define __BMS_ENERGY_H__

#include "bms_config.h"
#include "bms_global.h"
#include <stdbool.h>



typedef struct 
{
    float SocStopChg;        // 停止充电SOC
    float SocStartChg;       // 启动充电SOC
    float SocStopDsg;        // 停止放电SOC
    float SocStartDsg;       // 启动放电SOC

    float BalanceStartVoltage;  // 均衡起始电压
    float BalanceDiffeVoltage;  // 均衡差异电压
    BMS_CellIndexTypedef   BalanceCellRecord;  // 均衡记录的电池序号
    uint32_t BalanceCycleTime;  // 均衡周期时间
    bool BalanceReleaseFlag;    // 均衡释放标志,false:表示已不满足均衡条件,true:满足均衡条件

}BMS_EnergyDataTypedef;


extern BMS_EnergyDataTypedef BMS_EnergyData;

void BMS_EnergyInit(void);

#endif



