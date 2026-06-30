

#ifndef __BMS_HAL_CONTROL_H__
#define __BMS_HAL_CONTROL_H__

#include "bms_global.h"




void Bms_HalControlDischarge(BMS_StateTypeDef state);
void Bms_HalControlCharge(BMS_StateTypeDef state);
void Bms_HalControlBalance(BMS_CellIndexTypedef CellIndex, BMS_StateTypeDef NewState);



#endif


