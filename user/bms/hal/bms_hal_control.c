



#include "bms_hal_control.h"
#include "bq76920_iic.h"

#define DBG_TAG "hal"
#define DBG_LVL DBG_LOG
#include "rtdbg.h"




//控制放电开关
void Bms_HalControlDischarge(BMS_StateTypeDef state)
{

    BQ769X0_ControlDSGOrCHG(DSG_CONTROL, (BQ769X0_StateTypedef)state);
}


//控制充电开关
void Bms_HalControlCharge(BMS_StateTypeDef state)
{

    BQ769X0_ControlDSGOrCHG(CHG_CONTROL, (BQ769X0_StateTypedef)state);
}

//电池均衡

void Bms_HalControlBalance(BMS_CellIndexTypedef CellIndex, BMS_StateTypeDef NewState)
{

    BQ769X0_CellBalanceControl((BQ769X0_CellIndexTypedef) CellIndex, (BQ769X0_StateTypedef) NewState);
}

