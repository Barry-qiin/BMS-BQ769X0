


#include <rtthread.h>
#include <stdio.h>

#include "bms_info.h"
#include "bms_hal_mointor.h"
#include "bms_energy.h"
#include "bms_analysis.h"
#include "bms_mointor.h"
#include "bms_protect.h"
#include "bms_global.h"
#include "bms_utils.h"
//#include "bms_config.h"
#include "bms_hal_control.h"



#define DBG_TAG "cmd"
#define DBG_LVL DBG_LOG
#include "rtdbg.h"








static void BMS_CmdInfo(void)
{
    BMS_PrintfInfo();
}
MSH_CMD_EXPORT(BMS_CmdInfo, Print Info);


static void BMS_CmdOpenDSG(void)
{
    BMS_GlobalParam.Discharge = BMS_STATE_ENBLE;
}
MSH_CMD_EXPORT(BMS_CmdOpenDSG, Open DSG);


static void BMS_CmdCloseDSG(void)
{
    BMS_GlobalParam.Discharge = BMS_STATE_DISABLE;
}
MSH_CMD_EXPORT(BMS_CmdCloseDSG, Close DSG);



static void BMS_CmdOpenCHG(void)
{
    BMS_GlobalParam.Charge = BMS_STATE_ENBLE;
}
MSH_CMD_EXPORT(BMS_CmdOpenCHG, Open CHG);


static void BMS_CmdCloseCHG(void)
{
    BMS_GlobalParam.Charge = BMS_STATE_DISABLE;
}
MSH_CMD_EXPORT(BMS_CmdCloseCHG, Close CHG);





static void BMS_CmdOpenBalance(void)
{
	BMS_GlobalParam.Balance = BMS_STATE_ENBLE;
}
MSH_CMD_EXPORT(BMS_CmdOpenBalance, Open Balance);



static void BMS_CmdCloseBalance(void)
{
	BMS_GlobalParam.Balance = BMS_STATE_DISABLE;
}
MSH_CMD_EXPORT(BMS_CmdCloseBalance, Close Balance);




static void BMS_CmdLoadDetect(void)
{
	if (Bms_HalMonitorLoadDetect() == true)
	{
		LOG_I("Load Detected");
	}
	else
	{		
		LOG_I("No Load Was Detected");
	}
}
MSH_CMD_EXPORT(BMS_CmdLoadDetect, Load Detect);



