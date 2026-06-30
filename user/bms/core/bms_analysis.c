



#include <stdio.h>
#include <stdlib.h>
#include <rtthread.h>
#include <math.h>
#include "bms_analysis.h"
#include "bms_mointor.h"
#include "bms_protect.h"
#include "bms_global.h"
#include "bms_utils.h"
//#include "bms_config.h"



#define DBG_TAG "analysis"
#define DBG_LVL DBG_LOG
#include "rtdbg.h"


#define BMS_ANALYSIS_STACK_SIZE 256
#define BMS_ANALYSIS_PRIORITY   21
#define BMS_ANALYSIS_TIMESLICE  25

#define ANALYSIS_TASK_PERIOD  1000

#define TEMP_CAP_RATE_LIMITH_HIGH   1050
#define TEMP_CAP_RATE_LIMITL_LOW    750

BMS_AnalysisDataTypedef BMS_AnalysisData =
{
    .CapacityRated = BMS_BATTERY_CAPACITY,           
};

// 三元锂电池 SOC 开路电压法计算数据表
uint16_t SocOcvTab[101]=
{
	3282, // 0%~1%	
	3309, 3334, 3357, 3378, 3398, 3417, 3434, 3449, 3464, 3477,	// 0%~10%
	3489, 3500, 3510, 3520, 3528, 3536, 3543, 3549, 3555, 3561,	// 11%~20%
	3566, 3571, 3575, 3579, 3583, 3586, 3590, 3593, 3596, 3599,	// 21%~30%
	3602, 3605, 3608, 3611, 3615, 3618, 3621, 3624, 3628, 3632,	// 31%~40%
	3636, 3640, 3644, 3648, 3653, 3658, 3663, 3668, 3674, 3679,	// 41%~50%
	3685, 3691, 3698, 3704, 3711, 3718, 3725, 3733, 3741, 3748,	// 51%~60%
	3756, 3765, 3773, 3782, 3791, 3800, 3809, 3818, 3827, 3837,	// 61%~70%
	3847, 3857, 3867, 3877, 3887, 3897, 3908, 3919, 3929, 3940,	// 71%~80%
	3951, 3962, 3973, 3985, 3996, 4008, 4019, 4031, 4043, 4055,	// 81%~90%
	4067, 4080, 4092, 4105, 4118, 4131, 4145, 4158, 4172, 4185,	// 91~100%
};



static void BMS_AnalysisCapAndSocInit(void);
static void BMS_AnalysisCalCap(void);
static void BMS_AnalysisEasy(void);
static void BMS_AnalysisSocCheck(void);
static void BMS_Analysis_Entry(void *parameter);



void BMS_AnalysisInit(void)
{ 
    rt_thread_t thread;
    thread = rt_thread_create(  "analysis", 
                                BMS_Analysis_Entry, 
                                NULL, 
                                BMS_ANALYSIS_STACK_SIZE, 
                                BMS_ANALYSIS_PRIORITY , 
                                BMS_ANALYSIS_TIMESLICE);
    if(NULL == thread) 
    {
        LOG_E("Create Task Fail");

    }   

    rt_thread_startup(thread);

}



static void BMS_Analysis_Entry(void *parameter)
{
    BMS_AnalysisCapAndSocInit();
    while(1)
    {
        BMS_AnalysisEasy();
        BMS_AnalysisCalCap();
        BMS_AnalysisSocCheck();
        rt_thread_mdelay(ANALYSIS_TASK_PERIOD);
    }

}


// 根据单体电芯最低电压计算出soc值,用于上电和长时间静止状态下的校准
static uint16_t BMS_AnalysisOcvToSoc(uint16_t voltage)
{
    uint16_t soc = 0;
    if(voltage <= SocOcvTab[0])
    {
        soc = 0;
    }
    else if(voltage >= SocOcvTab[100])
    {
        soc = 1000;
    }
    else
    {
        uint16_t index = right_bound(SocOcvTab,0,100,voltage);
        if(voltage == SocOcvTab[index])
        {
            //整数
            soc = index * 10;
        }
        else
        {
            soc = (index-1)*10 +((voltage - SocOcvTab[index-1])*10) / (SocOcvTab[index]-SocOcvTab[index-1]);

        } 
    }

    return soc;
}


//静态情况下，开路电压计算法
static void BMS_AnalysisOcvS0cCal(void)
{ 
    // 进入睡眠的条件:待机一段时间以上且没有电池在均衡
    if(BMS_GlobalParam.SysMode == BMS_SYS_MODE_SLEEP)
    {
        // 等待一段时间电压平稳,防止均衡才刚结束
        rt_thread_mdelay(BALANCE_VOLT_RISE_DELAY);

    BMS_AnalysisData.SOC = BMS_AnalysisOcvToSoc(BMS_MointorData.CellData[0].CellVoltage * 1000) /1000;
    BMS_AnalysisData.CapacityRemain = BMS_AnalysisData.CapacityReal * BMS_AnalysisData.SOC ;
    }

}


// 安时积分法soc计算
// 待机模式下判断最低电压值是否大于等于过压保护值,成立则soc = 100%
// 待机模式下判断最低电压值是否小于等于欠压保护值,成立则soc = 0%
// 充电时对进行测量出来的电流值+积分
// 放电时对进行测量出来的电流值-积分
// soc = 实时积分的容量 / 电池包实际容量
static void BMS_AnalysisAHSocCalculate(void)
{
    //fabs取绝对值，除3600把 AS 单位换算成 Ah
    float CurrentValue = fabs(BMS_MointorData.BatteryCurrent) / 3600;

    if(BMS_GlobalParam.SysMode == BMS_SYS_MODE_STANDBY)
    {
        if(BMS_MointorData.CellData[0].CellVoltage >= BMS_Protect.Param.OV_PROTECT)
        {

            BMS_AnalysisData.SOC = 1;
        }
        else if(BMS_MointorData.CellData[0].CellVoltage <= BMS_Protect.Param.UV_PROTECT)
        {
            BMS_AnalysisData.SOC = 0;
        }
    }

    if(BMS_GlobalParam.SysMode == BMS_SYS_MODE_CHG)
    {

        if(BMS_AnalysisData.CapacityReal >= (BMS_AnalysisData.CapacityRemain + CurrentValue))
        {
            BMS_AnalysisData.CapacityRemain += CurrentValue;
        }
        else
        {
            BMS_AnalysisData.CapacityRemain = BMS_AnalysisData.CapacityReal;
        }
    }
    else if(BMS_GlobalParam.SysMode == BMS_SYS_MODE_DISCHG)
    {

        if(BMS_AnalysisData.CapacityRemain >= CurrentValue)
        {
            BMS_AnalysisData.CapacityRemain -= CurrentValue;
        }
        else
        {
            BMS_AnalysisData.CapacityRemain = 0;

        }
    }


    BMS_AnalysisData.SOC = BMS_AnalysisData.CapacityRemain / BMS_AnalysisData.CapacityReal;

    if(BMS_AnalysisData.SOC > 1)
    {
        BMS_AnalysisData.SOC = 1;
    }

}

// soc检查
static void BMS_AnalysisSocCheck(void)
{
    BMS_AnalysisOcvS0cCal();
    BMS_AnalysisAHSocCalculate();

}

//上电初始化SOC和容量
static void BMS_AnalysisCapAndSocInit(void)
{

    BMS_AnalysisData.SOC = BMS_AnalysisOcvToSoc(BMS_MointorData.CellData[0].CellVoltage * 1000) /1000.0;

    BMS_AnalysisData.CapacityReal = BMS_AnalysisData.CapacityRated;

    BMS_AnalysisData.CapacityRemain = BMS_AnalysisData.CapacityReal * BMS_AnalysisData.SOC ;
}

//简单数值分析

static void BMS_AnalysisEasy(void)
{ 
    //单体平均电压值(V)
    for(uint8_t i = 0; i < BMS_GlobalParam.CellRealNumber; i++)
    {
        BMS_AnalysisData.AverageVoltage += BMS_MointorData.CellVoltage[i];
    }
    BMS_AnalysisData.AverageVoltage /= BMS_GlobalParam.CellRealNumber;

    // 单体电池最大电压差值(V)
    BMS_AnalysisData.MaxVoltageDiffierence = BMS_MointorData.CellData[BMS_GlobalParam.CellRealNumber-1].CellVoltage - BMS_MointorData.CellData[0].CellVoltage;

    // 电池包实时功率(W)
    BMS_AnalysisData.PowerReal = BMS_MointorData.BatteryVoltage * BMS_MointorData.BatteryCurrent;

    //单体电芯最大电压(V)
    BMS_AnalysisData.CellVoltMax = BMS_MointorData.CellData[BMS_GlobalParam.CellRealNumber-1].CellVoltage;

    //单体电芯最小电压(V)
    BMS_AnalysisData.CellVoltMin = BMS_MointorData.CellData[0].CellVoltage;
}


// 温度校准
// 锂电池充放电时温度的变化会影响充放电时电压与时间的关系,进而影响电池实时容量
static void BMS_AnalysisCalTemp(void)
{ 
    static uint16_t LastTemp = 0;
    int16_t MinTemp = BMS_MointorData.CellTempera[0] * 10; //小数变成整数

    uint8_t Ratio = 0;
    uint16_t RatioTemp = 0;



    if( BMS_MointorData.CellTempEffectiveNumber == 0)
    {
        return;
    }
    //判断温度变化是否超过一度，未超过不进行校准
    if(abs(LastTemp - MinTemp) < 10)
    {
        return;
    }
    LastTemp = MinTemp;

    // 确定每一摄氏度的校准倍率
	// 该校准倍率的由来是根据不同温度下的放电曲线来的
	// 放电曲线：http://www.doczj.com/doc/1510977503.html
	// 上面链接的放电曲线跟这份代码的校准区间的参数有所不同	
	// 搜了几个三元锂电池的放电温度特性曲线,都是以25度常温为标准,25度时容量不受温度影响
    if(MinTemp >= 250)
    {
        // 温度大于25度时,每1度的倍率为0.001
		// 大于常温放电时间变长,就可以理解为容量增加
		// 增加的容量为：0.001 * (最小温度-常温)
        Ratio = 1;
    }
    else if(MinTemp >= 100 && MinTemp < 250)
    {
        // 25度以下,每1度的倍率为0.002
		// 容量增加为：0.002 * (最小温度-常温)
        Ratio = 2;
    }
    else if(MinTemp >= 0 && MinTemp < 100)
    {
        // 100度以下每1度的倍率为0.003
		// 容量增加为：0.003 * (最小温度-常温)
        Ratio = 3;
    }
    else if (MinTemp >= -200 && MinTemp < -10)
	{   
		Ratio = 4;
	}
	else if (MinTemp >= -300 && MinTemp < -200)
	{   
		Ratio = 5;
	}
	else
	{
        // ratio:这个量表示是一个变化趋势，温度越低，温度区间范围越大，也就证明变化趋势越大，所以ratio也在增大
		Ratio = 6;
	}

    /*该公式如何理解，(MinTemp - 250) * Ratio为影响的百分比，
    eg:10*1(原本是0.001，为了方便编译计算)=0.01，RatioTemp=1+0.01=1.01
    */
    RatioTemp = 1000 + (MinTemp - 250) / 10 * Ratio;

    //做个上限下限
    if(RatioTemp > TEMP_CAP_RATE_LIMITH_HIGH)
    {
        RatioTemp = TEMP_CAP_RATE_LIMITH_HIGH;
    }
    else if(RatioTemp < TEMP_CAP_RATE_LIMITL_LOW)
    {
        RatioTemp = TEMP_CAP_RATE_LIMITL_LOW;
    }

    //实时容量
    BMS_AnalysisData.CapacityReal = BMS_AnalysisData.CapacityRated * RatioTemp / 1000;
    //剩余容量
    BMS_AnalysisData.CapacityRemain = BMS_AnalysisData.CapacityReal * BMS_AnalysisData.SOC;

}


//实时校准容量，容量受温度，老化（完整充放电次数）影响
static void BMS_AnalysisCalCap(void)
{ 
    //这里只进行温度校准，老化因素带来的影响无数据支撑，暂未处理
    BMS_AnalysisCalTemp();
}




