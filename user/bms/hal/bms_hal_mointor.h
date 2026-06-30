



#ifndef __BMS_HAL_MOINTOR_H__
#define __BMS_HAL_MOINTOR_H__

#include <stdbool.h>

void Bms_HalMonitorCellVoltage(void);
void Bms_HalMonitorBatteryVoltage(void);
void Bms_HalMonitorBatteryCurrent(void);
void Bms_HalMonitorCellTemperature(void);
bool Bms_HalMonitorLoadDetect(void);





#endif



