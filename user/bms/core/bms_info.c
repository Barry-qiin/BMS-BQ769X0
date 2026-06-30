



#include <stdio.h>
#include <stdlib.h>
#include <rtthread.h>
#include <stdbool.h>
#include <math.h>
#include "main.h"

#include "bms_info.h"
#include "bms_energy.h"
#include "bms_analysis.h"
#include "bms_mointor.h"
#include "bms_protect.h"
#include "bms_global.h"
#include "bms_utils.h"
//#include "bms_config.h"
#include "bms_hal_control.h"


#define DBG_TAG "info"
#define DBG_LVL DBG_LOG
#include "rtdbg.h"



#define BMS_INFO_STACK_SIZE 512
#define BMS_INFO_PRIORITY   25
#define BMS_INFO_TIMESLICE  25

#define INFO_TASK_PERIOD  2000


static void BMS_InfoBatCapacityIndicator(void);
static void BMS_Info_Entry(void *parameter);
void BMS_InfoInit(void)
{ 
    rt_thread_t thread;
    thread = rt_thread_create(  "info", 
                                BMS_Info_Entry, 
                                NULL, 
                                BMS_INFO_STACK_SIZE, 
                                BMS_INFO_PRIORITY , 
                                BMS_INFO_TIMESLICE);
    if(NULL == thread) 
    {
        LOG_E("Create Task Fail");

    }   

    rt_thread_startup(thread);

}





static void BMS_Info_Entry(void *parameter)
{ 
    while(1)
    {

        BMS_InfoBatCapacityIndicator();
        rt_thread_mdelay(INFO_TASK_PERIOD);
    }

}


static void BMS_InfoBatCapacityIndicator(void)
{
    if(BMS_AnalysisData.SOC == 0)
    {
        HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, GPIO_PIN_SET);
    }
    else if(BMS_AnalysisData.SOC <= 0.25)
    {
        HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, GPIO_PIN_RESET);
    }
    else if(BMS_AnalysisData.SOC <= 0.5)
    {
        HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, GPIO_PIN_RESET);
    }
    else if(BMS_AnalysisData.SOC <= 0.75)
    {
        HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, GPIO_PIN_RESET);
    }
    else if(BMS_AnalysisData.SOC <= 1)
    {
        HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, GPIO_PIN_RESET);
    }
}


void BMS_PrintfInfo(void)
{ 
    char str[64];
    uint8_t index = 0;

    LOG_D("**********************************************************");
   
    // 电池包实际容量 
    sprintf(str, "Battery Real Capacity = %.3fAh", BMS_AnalysisData.CapacityReal); 
    LOG_D("%s",str);

    // 电池包剩余容量 
    sprintf(str, "Battery Remain Capacity = %.3fAh", BMS_AnalysisData.CapacityRemain); 
    LOG_D("%s",str);


    rt_kprintf("\r\n");


    // SOC
    sprintf(str, "Battery SOC = %.2f%%", BMS_AnalysisData.SOC * 100);
    LOG_D("%s",str);


    rt_kprintf("\r\n");


    //电芯最高电压
    sprintf(str, "Cell Voltage Max = %.3fV", BMS_AnalysisData.CellVoltMax);
    LOG_D("%s",str);

    //电芯最低电压
    sprintf(str, "Cell Voltage Min = %.3fV", BMS_AnalysisData.CellVoltMin);
    LOG_D("%s",str);

    //最大电压差
    sprintf(str, "Max Voltage Difference = %.3fV", BMS_AnalysisData.MaxVoltageDiffierence);
    LOG_D("%s",str);    

    //平均电压
    sprintf(str, "Average Voltage = %.3fV", BMS_AnalysisData.AverageVoltage);
    LOG_D("%s",str);

    //实时功率
    sprintf(str, "Power Real = %.3fW", BMS_AnalysisData.PowerReal);
    LOG_D("%s",str);
    

    rt_kprintf("\r\n");



    //电池包电压
    sprintf(str, "Battery Voltage = %.3fV", BMS_MointorData.BatteryVoltage);
    LOG_D("%s",str);

    //电池包电流
    sprintf(str, "Battery Current = %.3fA", BMS_MointorData.BatteryCurrent);
    LOG_D("%s",str);

    //温度
    for(index = 0; index < BMS_MointorData.CellTempEffectiveNumber; index++)
    {
        sprintf(str, "Temperature %d = %.2f", index + 1, BMS_MointorData.CellTempera[index]);
        LOG_D("%s",str);
    }



    rt_kprintf("\r\n");



    //电芯电压
    for(index = 0; index < BMS_GlobalParam.CellRealNumber; index++)
    {
        sprintf(str, "Cell%-2d Voltage  = %-5.3fV %s", index + 1, BMS_MointorData.CellVoltage[index],
        (BMS_EnergyData.BalanceCellRecord & (1 << index)) > 0 ? "balance" : "");
        LOG_D("%s",str);
    }

    LOG_D("**********************************************************");
}


