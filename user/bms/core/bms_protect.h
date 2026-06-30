


#ifndef __BMS_PROTECT_H__
#define __BMS_PROTECT_H__


#include "bms_config.h"


//保护相关参数，凡是在处配置的由硬件触发的保护参数配置在此处无作用，只有解除参数配置在软件作用下进行调用
typedef struct 
{
    float ShoutdownVoltage;	                    // 关机电压(V)

    BQ769X0_OVDelayTypedef OVDelay;             // 充电过压保护延时  硬件触发
    float OV_PROTECT;                           // 充电过压保护电压(V)  硬件触发        
    float OV_RELIEVE;                           // 充电过压恢复电压(V)

    BQ769X0_UVDelayTypedef UVDelay;             // 放电欠压保护延时  硬件触发
    float UV_PROTECT;                           // 放电欠压保护电压(V)  硬件触发 
    float UV_RELIEVE;                           // 放电欠压恢复电压(V)

    BQ769X0_SCDDelayTypedef SCDDelay;           // 放电短路保护延时 硬件触发
    BQ769X0_SCDThresholdTypedef SCDThreshold;   // 放电短路保护阈值(mV)，实质对应保护电流的变化 硬件触发
    uint8_t SCDDelay_RELIEVE;

    BQ769X0_OCDDelayTypedef OCDDelay;           // 放电过流保护延时 硬件触发
    BQ769X0_OCDThresholdTypedef OCDThreshold;   // 放电过流保护阈值(mV)，实质对应保护电流的变化 硬件触发
    uint8_t OCDDelay_RELIEVE;

    float OCC_PROTECT;                          // 充电过流保护电流(A)
    uint8_t OCC_DELAY;                          // 充电过流保护延时(S)
    uint8_t OCC_RELIEVE;                        // 充电过流解除时间(S)

    float OTC_PROTECT;                          // 充电过温保护(℃)
    float OTC_RELIEVE;                          // 充电过温解除(℃)

    float OTD_PROTECT;                          // 放电过温保护(℃)  
    float OTD_RELIEVE;                          // 放电过温解除(℃)

    float LTC_PROTECT;                          // 充电低温保护(℃)
    float LTC_RELIEVE;                          // 充电低温解除(℃)

    float LTD_PROTECT;                          // 放电低温保护(℃)
    float LTD_RELIEVE;                          // 放电低温解除(℃)  

}BMS_ProtectParamTypedef;



//保护触发标志位
typedef enum
{
    ALERT_FLAG_NO = 0x0000,

    ALERT_FLAG_OV = 0x0001,
    ALERT_FLAG_OCC = 0x0002,
    ALERT_FLAG_OTC = 0x0004,
    ALERT_FLAG_LTC = 0x0008,
    
    ALERT_CHG_MASK = 0x000F,	// 充电报警掩码

    ALERT_FLAG_UV = 0x0010,
    ALERT_FLAG_OCD = 0x0020,
    ALERT_FLAG_SCD = 0x0040,
    ALERT_FLAG_OTD = 0x0080,
    ALERT_FLAG_LTD = 0x0100,

    ALERT_DSG_MASK = 0x01F0,	// 放电报警掩码

}BMS_ProtectAlertTypedef;


typedef struct 
{
    BMS_ProtectAlertTypedef Alert;
    BMS_ProtectParamTypedef Param;

}BMS_ProtectTypedef;

extern BMS_ProtectTypedef BMS_Protect;




void BMS_ProtectInit(void);

void BMS_Protect_HWOCD(void);
void BMS_Protect_HWUV(void);
void BMS_Protect_HWOV(void);
void BMS_Protect_HWSCD(void);
void BMS_Protect_HwDevice(void);
void BMS_Protect_HwOvrd(void);


#endif

