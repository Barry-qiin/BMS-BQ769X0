
#include "bms_global.h"


BMS_GlobalParamTypedef BMS_GlobalParam = {
    .SysMode            = BMS_SYS_MODE_STANDBY,		// 当前系统模式
    .Charge             = BMS_STATE_DISABLE,       // 充电状态
    .Discharge          = BMS_STATE_DISABLE,    // 放电状态
    .Balance            = BMS_STATE_DISABLE,      // 均衡状态

    .CellRealNumber     = BMS_CELL_MAX,       // 实际电芯数
    .TempRealNumber     = BMS_TEMP_MAX,          // 实际温度数
};


