




#include <stdio.h>
#include <rtthread.h>
#include "bms_mointor.h"
#include "bms_hal_mointor.h"
#include "bms_global.h"
#include "bms_energy.h"

#define DBG_TAG "monitor"
#define DBG_LVL DBG_LOG
#include "rtdbg.h"

BMS_MointorDataTypedef BMS_MointorData;

//thread config
#define BMS_MONITOR_STACK_SIZE     512
#define BMS_MONITOR_PRIORITY       19
#define BMS_MONITOR_TIMESLICE      25

#define MONITOR_TASK_PERIOD        250

// sample time config  MS
#define UPDATE_CELL_VOLTAGE_CYCLE	250
#define UPDAYE_BAT_VOLTAGE_CYCLE	250
#define UPDATE_CELL_TEMP_CYCLE		2000
#define UPDATE_BAT_CURRENT_CYCLE	1000

//硬件采集触发CC采集电流标志位
static bool FlagSampleCC_Current = false;


static bool FlagCellVoltage = true;
static bool FlagBatteryVoltage = true;
static bool FlagCellTemperature = true;
static bool FlagBatteryCurrent = true;


static uint16_t CountCellVoltage = 0;
static uint16_t CountCellTemperature = 0;
//static uint16_t CountBatteryCurrent = 0;
static uint16_t CountBatteryVoltage = 0;


static void BMS_Mointor_Entry(void *parameter);
static void BMS_Mointor_Battery(void);
static void BMS_Mointor_SysMode(void);


void BMS_MointorInit(void)
{
    rt_thread_t thread;

    thread = rt_thread_create(  "mointor", 
                                BMS_Mointor_Entry, 
                                NULL, 
                                BMS_MONITOR_STACK_SIZE, 
                                BMS_MONITOR_PRIORITY , 
                                BMS_MONITOR_TIMESLICE);
    if(NULL == thread) 
    {
        LOG_E("Create Task Fail");

    }   

    rt_thread_startup(thread);

}


static void BMS_Mointor_Entry(void *parameter)
{ 
   // static uint8_t s_debug_cnt = 0;
    while(1)
    {
        BMS_Mointor_Battery();
        BMS_Mointor_SysMode();
 /*        // ============================================================
    // 【调试打印区域】 - 已优化格式并限制次数
    // ============================================================
    
    // 只有前 3 次进入这里才打印
    if(s_debug_cnt < 3)
    {
        if(s_debug_cnt == 0) 
        {
            LOG_W(">>> [DEBUG] BMS Monitor Start (Print 1/3) <<<");
        }
        else if (s_debug_cnt == 2)
        {
            LOG_W(">>> [DEBUG] BMS Monitor Init Done (Print 3/3) - Stopping Logs <<<");
        }

        // --- 1. 打印原始电芯电压 (Raw Data) ---
        LOG_W("-- Raw Cell Voltages --");
        for(uint8_t i = 0; i < BQ769X0_CELL_MAX; i++)
        {
            // 转换逻辑：float * 1000 -> int
            // 例如 3.650V -> 3650 -> 打印 "3.650"
            int32_t val_mv = (int32_t)(BMS_MointorData.CellVoltage[i] * 1000.0f + 0.5f);
            
            // 处理负数情况（虽然电池电压不会是负的，但这是通用写法）
            if(val_mv < 0) {
                LOG_W("  Cell[%d]: -%d.%03d V", i, (-val_mv)/1000, (-val_mv)%1000);
            } else {
                LOG_W("  Cell[%d]: %d.%03d V", i, val_mv/1000, val_mv%1000);
            }
        }

        // --- 2. 打印排序后的电芯数据 (Sorted Data) ---
        // 假设 BMS_MointorData.CellData 是排序后的结构体数组
        LOG_W("-- Sorted Cell Data --");
        for(uint8_t i = 0; i < BQ769X0_CELL_MAX; i++)
        {
            int32_t val_mv = (int32_t)(BMS_MointorData.CellData[i].CellVoltage * 1000.0f + 0.5f);
            LOG_W("  Rank[%d]: ID=%2d, Vol=%d.%03d V", 
                  i, 
                  BMS_MointorData.CellData[i].CellNumber, // 假设结构体里有编号
                  val_mv/1000, val_mv%1000);
        }

        // --- 3. 打印总电压和总电流 ---
        {
            int32_t pack_v_mv = (int32_t)(BMS_MointorData.BatteryVoltage * 1000.0f + 0.5f);
            int32_t pack_c_ma = (int32_t)(BMS_MointorData.BatteryCurrent * 1000.0f + 0.5f); // 假设电流单位是A
            
            LOG_W("-- Pack Status --");
            LOG_W("  Total Voltage: %d.%03d V", pack_v_mv/1000, pack_v_mv%1000);
            LOG_W("  Total Current: %d.%03d A", pack_c_ma/1000, pack_c_ma%1000);
        }

        // --- 4. 打印温度数据 ---
        LOG_W("-- Temperature (%d sensors) --", BMS_MointorData.CellTempEffectiveNumber);
        for(uint8_t i = 0; i < BMS_MointorData.CellTempEffectiveNumber; i++)
        {
            // 温度通常保留1位小数即可：* 10
            // 例如 25.4 C -> 254 -> 打印 "25.4"
            int32_t temp_x10 = (int32_t)(BMS_MointorData.CellTempera[i] * 10.0f + 0.5f);
            
            LOG_W("  Temp[%d]: %d.%d C", i, temp_x10/10, temp_x10%10);
        }
        
        LOG_W("-------------------------------");
        
        // 计数器加 1
        s_debug_cnt++;
    }
    // 如果 s_debug_cnt >= 3，上面的整个 if 块都会被跳过，不再打印任何内容 */
        rt_thread_mdelay(RT_TICK_PER_SECOND);
    }
}



//监控各项数据
static void BMS_Mointor_Battery(void)
{ 
    //电芯电压
    CountCellVoltage += MONITOR_TASK_PERIOD;
    if( FlagCellVoltage == true && CountCellVoltage >= UPDATE_CELL_VOLTAGE_CYCLE)
    {
        Bms_HalMonitorCellVoltage();
        CountCellVoltage = 0;

    }
    else if( FlagCellVoltage == false)
    {
        CountCellVoltage = 0;
    }

    //电池组电压
    CountBatteryVoltage += MONITOR_TASK_PERIOD;
    if( FlagBatteryVoltage == true && CountBatteryVoltage >= UPDAYE_BAT_VOLTAGE_CYCLE)
    {
        Bms_HalMonitorBatteryVoltage();
        CountBatteryVoltage = 0;
    }
    else if( FlagBatteryVoltage == false)
    {
        CountBatteryVoltage = 0;
    }

    //电池温度
    CountCellTemperature += MONITOR_TASK_PERIOD;
    if( FlagCellTemperature == true && CountCellTemperature >= UPDATE_CELL_TEMP_CYCLE)
    {
        Bms_HalMonitorCellTemperature();
        CountCellTemperature = 0;
    }
    else if( FlagCellTemperature == false)
    {
        CountCellTemperature = 0;
    }

    /*//电池电流,这里是软件控制。
    CountBatteryCurrent += MONITOR_TASK_PERIOD;
    if( FlagBatteryCurrent = true && CountBatteryCurrent >= UPDATE_BAT_CURRENT_CYCLE)
    {
        Bms_HalMonitorBatteryCurrent();
        CountBatteryCurrent = 0;
    }
    else if( FlagBatteryCurrent = false)
    {
        CountBatteryCurrent = 0;
    }
    */
    /*CC检测电流还有硬件中断触发模式，待补充*/
    if(FlagSampleCC_Current == true && FlagBatteryCurrent == true)
    {
        Bms_HalMonitorBatteryCurrent();
        FlagSampleCC_Current = false;
    }
}


// 系统模式监控
// BatteryCurrent > 20mA || BatteryCurrent < -20mA  处于非睡眠模式
// BatteryCurrent < 20mA || BatteryCurrent > -20mA  处于待机模式或者睡眠模式
// BatteryCurrent <= -20mA 处于放电模式
// BatteryCurrent >=  20mA 处于充电模式
// 20mA这个值根据最终硬件实测决定，测量电池未充放情况下系统静态功耗最大，不然会误触发进入模式
static void BMS_Mointor_SysMode(void)
{
    #define BMS_SYS_MODE_CURRENT_POSI  0.02f
    #define BMS_SYS_MODE_CURRENT_NEGAT  -0.02f

    static BMS_SysModeTypeDef SysModeBackup = BMS_SYS_MODE_NULLL;
    static uint32_t CountSysMode = 0;
    if(SysModeBackup != BMS_GlobalParam.SysMode)
    {
        switch(BMS_GlobalParam.SysMode)
        {
            case BMS_SYS_MODE_SLEEP:
                LOG_I("SysMode:Sleep");
                break ;
            case BMS_SYS_MODE_STANDBY:
                LOG_I("SysMode:Standby");
                break ;
            case BMS_SYS_MODE_DISCHG:
                LOG_I("SysMode:Dischg");
                break ;
            case BMS_SYS_MODE_CHG:
                LOG_I("SysMode:Chg");
                break ;
        }
        SysModeBackup = BMS_GlobalParam.SysMode;
    }

    switch(BMS_GlobalParam.SysMode)
    {
        case BMS_SYS_MODE_SLEEP:
            if(BMS_MointorData.BatteryCurrent >= BMS_SYS_MODE_CURRENT_POSI)
            {
                BMS_GlobalParam.SysMode = BMS_SYS_MODE_CHG;
            }
            else if(BMS_MointorData.BatteryCurrent <= BMS_SYS_MODE_CURRENT_NEGAT)
            {
                BMS_GlobalParam.SysMode = BMS_SYS_MODE_DISCHG;
            }
            break ;
        case BMS_SYS_MODE_STANDBY:
            if(BMS_MointorData.BatteryCurrent >= BMS_SYS_MODE_CURRENT_POSI)
            {
                BMS_GlobalParam.SysMode = BMS_SYS_MODE_CHG;
            }
            else if(BMS_MointorData.BatteryCurrent <= BMS_SYS_MODE_CURRENT_NEGAT)
            {
                BMS_GlobalParam.SysMode = BMS_SYS_MODE_DISCHG;
            }

            CountSysMode += MONITOR_TASK_PERIOD;
            if(CountSysMode >= BMS_ENTRY_SLEEP_TIME *60 *1000)
            {
                /*这里还需要一个判断是否均衡是否进行而再进入睡眠模式*/
                if (BMS_EnergyData.BalanceReleaseFlag != true)
                {
                    BMS_GlobalParam.SysMode = BMS_SYS_MODE_SLEEP;
                    CountSysMode = 0;/* code */
                }
            }

            break;

        case BMS_SYS_MODE_CHG:
            if(BMS_MointorData.BatteryCurrent < BMS_SYS_MODE_CURRENT_POSI)
            {
                if(BMS_MointorData.BatteryCurrent > BMS_SYS_MODE_CURRENT_NEGAT)
                {
                    BMS_GlobalParam.SysMode = BMS_SYS_MODE_STANDBY;
                }
                else if(BMS_MointorData.BatteryCurrent <= BMS_SYS_MODE_CURRENT_NEGAT)
                {
                    BMS_GlobalParam.SysMode = BMS_SYS_MODE_DISCHG;
                }
            }
            break;
        
        case BMS_SYS_MODE_DISCHG:
            if(BMS_MointorData.BatteryCurrent > BMS_SYS_MODE_CURRENT_NEGAT)
            {
                if(BMS_MointorData.BatteryCurrent < BMS_SYS_MODE_CURRENT_POSI)
                {
                    BMS_GlobalParam.SysMode = BMS_SYS_MODE_STANDBY;
                }
                else if(BMS_MointorData.BatteryCurrent >= BMS_SYS_MODE_CURRENT_POSI)
                {
                    BMS_GlobalParam.SysMode = BMS_SYS_MODE_CHG;
                }
            }
            break;
        case BMS_SYS_MODE_NULLL:
            break;    
    }
}


void BMS_Mointor_HWCurrent(void)
{
    FlagSampleCC_Current = true;

}




