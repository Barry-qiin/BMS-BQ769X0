




#include "bsp_app.h"

#include "bq76920_iic.h"
#include "drv_iic.h"
#include "bms_config.h"
#include "bms_mointor.h"
#include "bms_protect.h"
#include "bms_analysis.h"
#include "bms_info.h"
#include "bms_energy.h"




void BMS_SysInitialize(void)
{
    BQ769X0_InitDataTypedef InitData;

    InitData.AlertOps.ocd = BMS_Protect_HWOCD;
    InitData.AlertOps.scd = BMS_Protect_HWSCD;
    InitData.AlertOps.ov = BMS_Protect_HWOV;
    InitData.AlertOps.uv = BMS_Protect_HWUV;

    InitData.AlertOps.ovrd = BMS_Protect_HwOvrd;
    InitData.AlertOps.device = BMS_Protect_HwDevice;

    InitData.AlertOps.cc = BMS_Mointor_HWCurrent;

    InitData.ConfigData.SCDDelay        = INIT_SCD_DELAY;
    InitData.ConfigData.OCDDelay        = INIT_OCD_DELAY;
    InitData.ConfigData.SCDThreshold    = INIT_SCDThreshold;
    InitData.ConfigData.OCDThreshold    = INIT_OCDThreshold;
    InitData.ConfigData.UVDelay         = INIT_UV_DELAY;
    InitData.ConfigData.OVDelay         = INIT_OV_DELAY;
    InitData.ConfigData.UVPThreshold    = INIT_UV_PROTECT * 1000;
    InitData.ConfigData.OVPThreshold    = INIT_OV_PROTECT * 1000;

    iic_BUS_init();
    //I2C_BusInitialize();
    BQ769X0_Init(&InitData);

    BMS_MointorInit();
    BMS_ProtectInit();
    BMS_AnalysisInit();
    BMS_EnergyInit();
    BMS_InfoInit();
}











