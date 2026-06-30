



#include <stdio.h>
#include <stdbool.h>
#include <rtthread.h>
#include "bms_protect.h"
#include "bms_global.h"
#include "bms_mointor.h"
#include "bms_hal_control.h"

#define DBG_TAG "protect"
#define DBG_LVL DBG_LOG
#include "rtdbg.h"


#define PROTECT_TASK_PERIOD  200


#define BMS_PROTECT_STACK_SIZE  512
#define BMS_PROTECT_PRIORITY    20
#define BMS_PROTECT_TIMESLICE   25 



BMS_ProtectTypedef BMS_Protect = 
{  
    .Alert = ALERT_FLAG_NO,
    .Param =
    {
        .ShoutdownVoltage   = INIT_SHUTDOWN_VOLTAGE,               

        .OVDelay            = INIT_OV_DELAY,           
        .OV_PROTECT         = INIT_OV_PROTECT,                                   
        .OV_RELIEVE         = INIT_OV_RELIEVE,                          

        .UVDelay            = INIT_UV_DELAY,             
        .UV_PROTECT         = INIT_UV_PROTECT,                            
        .UV_RELIEVE         = INIT_UV_RELIEVE,                           

        .SCDDelay           = INIT_SCD_DELAY,          
        .SCDThreshold       = INIT_SCDThreshold,   
        .SCDDelay_RELIEVE   = INIT_SCD_RELIEVE,

        .OCDDelay           = INIT_OCD_DELAY,          
        .OCDThreshold       = INIT_OCDThreshold,   
        .OCDDelay_RELIEVE   = INIT_OCD_RELIEVE,

        .OCC_PROTECT        = INIT_OCC_MAX,                          
        .OCC_DELAY          = INIT_OCC_DELAY,                          
        .OCC_RELIEVE        = INIT_OCC_RELIEVE,                        

        .OTC_PROTECT        = INIT_OTC_PROTECT ,                         
        .OTC_RELIEVE        = INIT_OTC_RELIEVE ,                         

        .OTD_PROTECT        = INIT_OTD_PROTECT ,                         
        .OTD_RELIEVE        = INIT_OTD_RELIEVE,                       

        .LTC_PROTECT        = INIT_LTC_PROTECT ,                       
        .LTC_RELIEVE        = INIT_LTC_RELIEVE ,                      

        .LTD_PROTECT        = INIT_LTD_PROTECT ,                     
        .LTD_RELIEVE        = INIT_LTD_RELIEVE ,                       

    }
};


static void BMS_Protect_Entry(void *parameter);
static void BMS_Protect_Trigger(void);
static void BMS_Protect_Relieve(void);

void BMS_ProtectInit(void)
{
    rt_thread_t thread;

    thread = rt_thread_create(  "protect", 
                                BMS_Protect_Entry, 
                                NULL, 
                                BMS_PROTECT_STACK_SIZE, 
                                BMS_PROTECT_PRIORITY , 
                                BMS_PROTECT_TIMESLICE);
    if(NULL == thread) 
    {
        LOG_E("Create Task Fail");

    }   

    rt_thread_startup(thread);

}

static void BMS_Protect_Entry(void *parameter)
{
    while(1)
    {
        BMS_Protect_Trigger();
        BMS_Protect_Relieve();
        rt_thread_mdelay(PROTECT_TASK_PERIOD);
    }
}


static void BMS_Protect_Trigger(void)
{
   static uint32_t ProtectCount = 0;
   
    switch(BMS_GlobalParam.SysMode) 
   {
       case BMS_SYS_MODE_CHG:
       {
            //充电过流保护
            if(BMS_MointorData.BatteryCurrent > BMS_Protect.Param.OCC_PROTECT) 
            {
                ProtectCount +=PROTECT_TASK_PERIOD;
                if(ProtectCount / 1000 >= BMS_Protect.Param.OCC_DELAY)
                {
                    Bms_HalControlCharge(BMS_STATE_DISABLE);
                    BMS_Protect.Alert = ALERT_FLAG_OCC;

                    LOG_W("Charge:OCC Protect Trigger");
                }
                 ProtectCount = 0;  
            }
            //充电过温
            else if(BMS_MointorData.CellTempEffectiveNumber == 0)
            {
                //有效温度值数量为0
                return;    
            }
           else if(BMS_MointorData.CellTempera[BMS_MointorData.CellTempEffectiveNumber-1] > BMS_Protect.Param.OTC_PROTECT) 
           { 
                Bms_HalControlCharge(BMS_STATE_DISABLE);
                BMS_Protect.Alert = ALERT_FLAG_OTC;

                LOG_W("Charge:OTC Protect Trigger");
           }
           //充电低温
           else if(BMS_MointorData.CellTempera[0] < BMS_Protect.Param.LTC_PROTECT) 
           { 
                Bms_HalControlCharge(BMS_STATE_DISABLE);
                BMS_Protect.Alert = ALERT_FLAG_LTC;

                LOG_W("Discharge:LTC Protect Trigger");
           } 
       } 
           break;

       case BMS_SYS_MODE_DISCHG:
        {
            //放电过温保护
            if(BMS_MointorData.CellTempEffectiveNumber == 0)
            {
                //有效温度值数量为0
                return;    
            }
            else if(BMS_MointorData.CellTempera[BMS_MointorData.CellTempEffectiveNumber-1] > BMS_Protect.Param.OTD_PROTECT) 
            { 
                Bms_HalControlDischarge(BMS_STATE_DISABLE);
                BMS_Protect.Alert = ALERT_FLAG_OTD;

                LOG_W("Discharge:OTD Protect Trigger");
            }
            //放电低温
            else if(BMS_MointorData.CellTempera[0] < BMS_Protect.Param.LTD_PROTECT) 
            { 
                Bms_HalControlDischarge(BMS_STATE_DISABLE);
                BMS_Protect.Alert = ALERT_FLAG_LTD;

                LOG_W("Discharge:LTD Protect Trigger");
            }
        }
            break;
            
       default:
           break;
   }
}



static void BMS_Protect_Relieve(void)
{
    static uint32_t CHGRelieveCount = 0,DSGRelieveCount = 0;
    if(BMS_Protect.Alert != ALERT_FLAG_NO)
    {
        if(BMS_Protect.Alert & ALERT_FLAG_OV)
        {
            if(BMS_MointorData.CellData[BQ769X0_CELL_MAX-1].CellVoltage < BMS_Protect.Param.OV_RELIEVE)
            {
                BMS_Protect.Alert &= ~ALERT_FLAG_OV;    // 软件层面删除过压报警，这里不操控硬件停止充电是因为，硬件会自动相应关闭的。

                LOG_I("Charge:OV Relieve");
            }
        }
        else if(BMS_Protect.Alert & ALERT_FLAG_OCC)
        {
            CHGRelieveCount +=PROTECT_TASK_PERIOD;
            if(CHGRelieveCount / 1000 >= BMS_Protect.Param.OCC_RELIEVE)
            {
                BMS_Protect.Alert &= ~ALERT_FLAG_OCC;    
                Bms_HalControlCharge(BMS_GlobalParam.Charge);
                LOG_I("Charge:OCC Relieve");
                CHGRelieveCount = 0;
            }
        }
        else if(BMS_Protect.Alert & ALERT_FLAG_OTC)
        {
            if(BMS_MointorData.CellTempera[BMS_TEMP_MAX-1] < BMS_Protect.Param.OTC_RELIEVE) 
            { 
                BMS_Protect.Alert &= ~ALERT_FLAG_OTC;    
                Bms_HalControlCharge(BMS_GlobalParam.Charge);
                LOG_I("Charge:OTC Relieve");
            }
        }
        else if(BMS_Protect.Alert & ALERT_FLAG_LTC)
        {
            if(BMS_MointorData.CellTempera[0] > BMS_Protect.Param.LTC_RELIEVE) 
            { 
                BMS_Protect.Alert &= ~ALERT_FLAG_LTC;    
                Bms_HalControlCharge(BMS_GlobalParam.Charge);
                LOG_I("Charge:LTC Relieve");
            }
        }



        if(BMS_Protect.Alert & ALERT_FLAG_UV)
        {
            if(BMS_MointorData.CellData[0].CellVoltage > BMS_Protect.Param.UV_RELIEVE)
            {
                BMS_Protect.Alert &= ~ALERT_FLAG_UV;    // 软件层面删除欠压报警，这里不操控硬件停止充放电是因为，硬件会自动相应关闭的。

                LOG_I("Discharge:UV Relieve");
            }
        }
        else if(BMS_Protect.Alert & ALERT_FLAG_OCD)
        {
            DSGRelieveCount +=PROTECT_TASK_PERIOD;
            if(DSGRelieveCount/1000 >= BMS_Protect.Param.OCDDelay_RELIEVE) 
            { 
                BMS_Protect.Alert &= ~ALERT_FLAG_OCD;
                Bms_HalControlDischarge(BMS_GlobalParam.Discharge);
                LOG_I("Discharge:OCD Relieve");
                DSGRelieveCount = 0;
            }
        }
        else if(BMS_Protect.Alert & ALERT_FLAG_SCD)
        {
            DSGRelieveCount +=PROTECT_TASK_PERIOD;
            if(DSGRelieveCount/1000 >= BMS_Protect.Param.SCDDelay_RELIEVE) 
            { 
                BMS_Protect.Alert &= ~ALERT_FLAG_SCD;
                Bms_HalControlDischarge(BMS_GlobalParam.Discharge);
                LOG_I("Discharge:SCD Relieve");
                DSGRelieveCount = 0;
            }
        }
        else if(BMS_Protect.Alert & ALERT_FLAG_OTD)
        {
            if(BMS_MointorData.CellTempera[BMS_TEMP_MAX-1] < BMS_Protect.Param.OTD_RELIEVE) 
            { 
                BMS_Protect.Alert &= ~ALERT_FLAG_OTD;    
                Bms_HalControlDischarge(BMS_GlobalParam.Discharge);
                LOG_I("Discharge:OTD Relieve");
            }
        }
        else if(BMS_Protect.Alert & ALERT_FLAG_LTD)
        {
            if(BMS_MointorData.CellTempera[0] > BMS_Protect.Param.LTD_RELIEVE) 
            { 
                BMS_Protect.Alert &= ~ALERT_FLAG_LTD;    
                Bms_HalControlDischarge(BMS_GlobalParam.Discharge);
                LOG_I("Discharge:LTD Relieve");
            }
        }
    }
}





/*void (*ocd)(void);		// BQ769X0 放电过流硬件报警
	void (*scd)(void);		// BQ769X0 放电电路硬件报警
	void (*ov)(void);		// BQ769X0 充电过压硬件报警
	void (*uv)(void);		// BQ769X0 放电欠压硬件报警
	void (*ovrd)(void);		// BQ769X0 报警引脚由用户外围电路强行触发
	void (*device)(void);	// BQ769X0 设备故障报警
	void (*cc)(void);		// BQ769X0 库仑计采样完成*/

 //ocd硬件触发
 void BMS_Protect_HWOCD(void)
 {
    if((BMS_Protect.Alert & ALERT_FLAG_OCD) == ALERT_FLAG_NO)    //防止多次触发，说明未被触发再执行下面
    {
        Bms_HalControlDischarge(BMS_STATE_DISABLE);
        BMS_Protect.Alert |= ALERT_FLAG_OCD;
        LOG_W("Discharge:OCD Hardware Trigger");
    }
 }  
 
 //scd硬件触发
 void BMS_Protect_HWSCD(void)
 {
    if((BMS_Protect.Alert & ALERT_FLAG_SCD) == ALERT_FLAG_NO)    //防止多次触发，说明未被触发再执行下面
    {
        Bms_HalControlDischarge(BMS_STATE_DISABLE);
        BMS_Protect.Alert |= ALERT_FLAG_SCD;
        LOG_W("Discharge:SCD Hardware Trigger");
    }
 }

 //ov硬件触发
 void BMS_Protect_HWOV(void)
 {
    if((BMS_Protect.Alert & ALERT_FLAG_OV) == ALERT_FLAG_NO)    //防止多次触发，说明未被触发再执行下面
    {
        Bms_HalControlCharge(BMS_STATE_DISABLE);
        BMS_Protect.Alert |= ALERT_FLAG_OV;
        LOG_W("Charge:OV Hardware Trigger");
    }
 }

 //uv硬件触发
 void BMS_Protect_HWUV(void)
 {
    if((BMS_Protect.Alert & ALERT_FLAG_UV) == ALERT_FLAG_NO)    //防止多次触发，说明未被触发再执行下面
    {
        Bms_HalControlDischarge(BMS_STATE_DISABLE);
        BMS_Protect.Alert |= ALERT_FLAG_UV;
        LOG_W("Discharge:UV Hardware Trigger");
    }
 }


 void BMS_Protect_HwDevice(void)
{
	LOG_W("BMS_Protect_HwDevice");
}

void BMS_Protect_HwOvrd(void)
{
	LOG_W("BMS_Protect_HwOvrd");
}




