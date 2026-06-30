


#include <stdio.h>
#include <stdlib.h>
#include <rtthread.h>
#include <stdbool.h>
#include <math.h>

#include "bms_energy.h"
#include "bms_analysis.h"
#include "bms_mointor.h"
#include "bms_protect.h"
#include "bms_global.h"
#include "bms_utils.h"
//#include "bms_config.h"
#include "bms_hal_control.h"

#define DBG_TAG "energy"
#define DBG_LVL DBG_LOG
#include "rtdbg.h"


BMS_EnergyDataTypedef BMS_EnergyData =
{
    .SocStopChg             = SOC_STOP_CHG_VALUE,
    .SocStartChg            = SOC_START_CHG_VALUE,
    .SocStopDsg             = SOC_STOP_DSG_VALUE,
    .SocStartDsg            = SOC_START_DSG_VALUE,

    .BalanceStartVoltage    = INIT_BALANCE_VOLTAGE,
    .BalanceDiffeVoltage    = BALANCE_DIFFE_VOLTAGE,
    .BalanceCellRecord      = BMS_CELL_NULL,
    .BalanceCycleTime       = BALANCE_CYCLE_TIME,
};

#define BMS_ENERGY_STACK_SIZE 512
#define BMS_ENERGY_PRIORITY   22
#define BMS_ENERGY_TIMESLICE  25

#define ENERGY_TASK_PERIOD  200

static rt_timer_t pTimerBalance;
static uint32_t BalanceVoltRiseTime;
static bool BalanceStartFlag = false;

static void BMS_Energy_Entry(void *parameter);
static void BMS_BalanceTimer_Entry(void *parameter);
static void  BMS_BalanceManage(void);
static void BMS_ChgDsgManage(void);
void BMS_EnergyInit(void)
{ 
    rt_thread_t thread;
    thread = rt_thread_create(  "energy", 
                                BMS_Energy_Entry, 
                                NULL, 
                                BMS_ENERGY_STACK_SIZE, 
                                BMS_ENERGY_PRIORITY , 
                                BMS_ENERGY_TIMESLICE);
    if(NULL == thread) 
    {
        LOG_E("Create Task Fail");

    }   

    rt_thread_startup(thread);

    pTimerBalance = rt_timer_create("Balance", 
                                    BMS_BalanceTimer_Entry, 
                                    NULL, 
                                    20, 
                                    RT_TIMER_FLAG_ONE_SHOT | RT_TIMER_FLAG_SOFT_TIMER);

    if (pTimerBalance == NULL)
	{
		LOG_E("Create Timer Fail");
	}                                
}



static void BMS_Energy_Entry(void *parameter)
{

    while(1)
    {
        BMS_BalanceManage();
        BMS_ChgDsgManage();
        rt_thread_mdelay(ENERGY_TASK_PERIOD);
    }
}

//充放电管理
static void BMS_ChgDsgManage(void)
{
    switch(BMS_GlobalParam.SysMode)
    {
        case BMS_SYS_MODE_STANDBY:
        {
            if(BMS_GlobalParam.Charge == BMS_STATE_ENBLE)
            {
                if((BMS_Protect.Alert & ALERT_CHG_MASK) ==  ALERT_FLAG_NO)
                {
                   if(BMS_EnergyData.BalanceReleaseFlag != true)
                   {
                       if(BMS_AnalysisData.SOC < BMS_EnergyData.SocStartChg)
                       {
                           Bms_HalControlCharge(BMS_STATE_ENBLE);
                           LOG_I("Charge Start");
                       }
                   } 
                }
            }
            if(BMS_GlobalParam.Discharge == BMS_STATE_ENBLE)
            {
                if((BMS_Protect.Alert & ALERT_DSG_MASK) ==  ALERT_FLAG_NO)
                {
                    if(BMS_AnalysisData.SOC > BMS_EnergyData.SocStartDsg)
                    {
                        Bms_HalControlDischarge(BMS_STATE_ENBLE);
                        LOG_I("Discharge Start");
                    }
                }
            }
        }
        break;
        case BMS_SYS_MODE_CHG:
        {   
            if(BMS_AnalysisData.SOC >= BMS_EnergyData.SocStopChg)
            {
                Bms_HalControlCharge(BMS_STATE_DISABLE);
                LOG_I("Charge Stop");
            }
            //这里是通过命令快速控制关闭充电
            else if(BMS_GlobalParam.Charge == BMS_STATE_DISABLE)
            {
                Bms_HalControlCharge(BMS_STATE_DISABLE);
                LOG_I("Charge Stop");
            }
        }
        break;
        case BMS_SYS_MODE_DISCHG:
        {
            if(BMS_AnalysisData.SOC <= BMS_EnergyData.SocStopDsg)
            {
                Bms_HalControlDischarge(BMS_STATE_DISABLE);
                LOG_I("Discharge Stop");
            }
            //这里是通过命令快速控制关闭放电
            else if(BMS_GlobalParam.Discharge == BMS_STATE_DISABLE)
            {
                Bms_HalControlDischarge(BMS_STATE_DISABLE);
                LOG_I("Discharge Stop");
            }
        }
        break;
        case BMS_SYS_MODE_SLEEP:
        {
           //睡眠模式下，通过命令快速控制启动充放电
            if(BMS_GlobalParam.Discharge == BMS_STATE_ENBLE)
            {
                Bms_HalControlDischarge(BMS_STATE_ENBLE);
                LOG_I("Discharge Start");
            }
            if(BMS_GlobalParam.Charge == BMS_STATE_ENBLE)
            {
                Bms_HalControlCharge(BMS_STATE_ENBLE);
                LOG_I("Charge Start");
            }    
        }
        break;
        default:
        break;
    }
}




//定时器回调函数入口，定时器结束后执行该函数，也就是一个均衡周期结束了
static void BMS_BalanceTimer_Entry(void *parameter)
{
    
    Bms_HalControlBalance(BMS_CELL_ALL, BMS_STATE_DISABLE);
    BalanceStartFlag = false;
    BMS_EnergyData.BalanceCellRecord = BMS_CELL_NULL;
    //均衡完成后的均衡回压时间
    BalanceVoltRiseTime = rt_tick_from_millisecond(BALANCE_VOLT_RISE_DELAY) + rt_tick_get();

    LOG_I("Balance Timer End");
}

//均衡启动条件检查
static bool BMS_BalanceCheck(void)
{
    //上一轮均衡回压时间还未结束
    if(BalanceVoltRiseTime >= rt_tick_get())
    {
        return false;
    }
    //均衡标志位,定时器启动
    if(BalanceStartFlag != false)
    { 
        return false;
    }
    //均衡未使能
    if(BMS_GlobalParam.Balance == BMS_STATE_DISABLE)
    {
        return false;
    }
    //未处于待机模式和充电模式
    if(BMS_GlobalParam.SysMode != BMS_SYS_MODE_STANDBY && BMS_GlobalParam.SysMode != BMS_SYS_MODE_CHG)
    {
        BMS_EnergyData.BalanceReleaseFlag = false;
        return false;
    }
    //最高电芯电压小于均衡起始电压
    if(BMS_AnalysisData.CellVoltMax < BMS_EnergyData.BalanceStartVoltage)
    {
        BMS_EnergyData.BalanceReleaseFlag = false;
        return false;
    }
    //最高和最低电压差未达到均衡差异电压
    if(BMS_AnalysisData.MaxVoltageDiffierence < BMS_EnergyData.BalanceDiffeVoltage)
    {
        BMS_EnergyData.BalanceReleaseFlag = false;
        return false;
    }
    
    BMS_EnergyData.BalanceReleaseFlag = true;
    return true;
}


//均衡电池筛选
static void BMS_BalanceFilter(void)
{
  //基于BQ76920，适用相邻电池不能均衡的筛选，按照电压从大到小的顺序
    float MinVoltage = BMS_MointorData.CellData[0].CellVoltage;
    float CampareVoltage = 0;
    uint8_t index = 0;
    uint8_t CellNumber = 0;
    bool result = false;

    for(index = 1; index < BMS_GlobalParam.CellRealNumber + 1; index++)
    {
        CampareVoltage = BMS_MointorData.CellData[BMS_GlobalParam.CellRealNumber -index].CellVoltage;
        if((CampareVoltage - MinVoltage) >BMS_EnergyData.BalanceDiffeVoltage)
        {
            result = false;
            CellNumber = BMS_MointorData.CellData[BMS_GlobalParam.CellRealNumber -index].CellNumber;
            
            if(CellNumber == 0)
            {
                //第一节满足均衡，看第二节是否被均衡了均衡标志
                if((BMS_EnergyData.BalanceCellRecord & BMS_CELL_INDEX2) == 0)
                {
                    result = true;
                }
            }
            else if(CellNumber + 1 == BMS_GlobalParam.CellRealNumber)
            {
                //最后一节满足均衡，看前一节是否被均衡了均衡标志
                if((BMS_EnergyData.BalanceCellRecord & (1 << (CellNumber - 1))) == 0)
                {
                    result = true;
                }
            }
            else
            {
                //中间节满足均衡，看前后节是否被均衡了均衡标志
                if((BMS_EnergyData.BalanceCellRecord & (1 << (CellNumber - 1))) == 0 && (BMS_EnergyData.BalanceCellRecord & (1 << (CellNumber + 1))) == 0)
                {
                    result = true;
                }
            }


            if(result == true)
            {
                LOG_I("Balance Cell:%d", CellNumber + 1);
                BMS_EnergyData.BalanceCellRecord |= (1 << CellNumber);
            }
        }
    }
}

static void BMS_BalanceTimerStart(uint32_t time)
{
    uint32_t tick = 0;

    tick = rt_tick_from_millisecond(time * 1000);
    rt_timer_control(pTimerBalance, RT_TIMER_CTRL_SET_TIME, &tick);
    rt_timer_start(pTimerBalance);
}

//均衡启动
static void BMS_BalanceStart(void)
{
   if(BMS_EnergyData.BalanceCellRecord != BMS_CELL_NULL)
   {
        Bms_HalControlBalance(BMS_EnergyData.BalanceCellRecord, BMS_STATE_ENBLE);
        BMS_BalanceTimerStart(BMS_EnergyData.BalanceCycleTime);
        BalanceStartFlag = true;  
        LOG_I("Balance Start");
   }

}

//均衡管理配置
static void  BMS_BalanceManage(void)
{
    if(BMS_BalanceCheck() == true)
    {
        BMS_BalanceFilter();
        BMS_BalanceStart();
    }

}



