#include "bq76920_iic.h"
//#include "drv_soft_i2c.h"
#include <math.h>


#define DBG_TAG "bq76920"
#define DBG_LVL DBG_LOG
#include "rtdbg.h"

static RegisterGroup Registers = {0};

static BQ769X0_AlertOpsTypedef	AlertOps;

/* ADC增益 */
static float Gain = 0;		
static int16_t iGain = 0;
static int8_t Adcoffset;

// 分流电阻阻值,单位毫欧
static float RsnsValue= 0.005;

BQ769X0_SampleDataTypedef BQ769X0_SampleData = {0}; 

// 热敏电阻阻值换算成温度
static float TempChange(float	Rt)
{
	float temp = 0;

	// 热敏电阻在T2常温下的标称阻值,我买的是10K
	float Rp = 10000;

	// 该热敏电阻在开尔文温度下的,热敏电阻阻值为10K时对应的温度为25度
	float T2 = 273.15 + 25;

	// B值:3935、3950
	float Bx = 3950;

	// 开尔文温度值
	float Ka = 273.15;

	// 打印出热敏电阻的实时阻值可与购买链接的阻值与温度对应表对照查看
	//sprintf((char *)buffer, "%f", Rt);
	//BQ769X0_INFO("Rts value:%s", buffer);

	temp = 1 / (1 / T2 + log(Rt / Rp) / Bx)- Ka + 0.5;

	return temp;
}


// CRC8校验
static uint8_t CRC8(uint8_t *ptr, uint8_t len, uint8_t key)
{
	uint8_t i, crc=0;
	
	while (len-- != 0)
	{
		for (i = 0x80; i != 0; i /= 2)
		{
			if ((crc & 0x80) != 0)
			{
				crc *= 2;
				crc ^= key;
			}
			else
			{
				crc *= 2;
			}

			if ((*ptr & i) != 0)
			{
				crc ^= key;
			}
		}
		ptr++;
	}
	return(crc);
}

//bq76920报警处理函数声明
static void BQ769X0_AlertyHandler(void);


static void Q769X0_TS1_WACKUP_SetOutMode(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    GPIO_InitStruct.Pin = BQ76920_TS1_WACKUP_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(BQ76920_TS1_WACKUP_GPIO_Port, &GPIO_InitStruct);
}

static void Q769X0_TS1_WACKUP_SetInMode(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    GPIO_InitStruct.Pin = BQ76920_TS1_WACKUP_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(BQ76920_TS1_WACKUP_GPIO_Port, &GPIO_InitStruct);
}

//alert报警回调函数
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if(GPIO_Pin == ALERT_Pin)
    {
        BQ769X0_AlertyHandler();
    }
}

/*******************寄存器的读写配置函数****************************/

static bool BQ769X0_WriteRegisterByte(uint8_t Register, uint8_t data)
{
    uint8_t dataBuffer[2] = {Register, data};
    struct IIC_MessagetypeDef msg = {0};

    msg.addr = BQ769X0_I2C_ADDR;
    msg.buf = dataBuffer;
    msg.flags = I2C_WR;
    msg.tx_len = 2;

    if(iic_transfermessage(&I2C_1, &msg, 1) != 1)
    {
        LOG_E("Write Register Byte Fail");

		return false;
    }

    return true;
}

static bool BQ769X0_WriteRegisterByteWithCRC(uint8_t Register, uint8_t data)
{
    uint8_t dataBuffer[4];
    struct IIC_MessagetypeDef msg = {0};

    dataBuffer[0] = BQ769X0_I2C_ADDR << 1;
    dataBuffer[1] = Register;
    dataBuffer[2] = data;
    dataBuffer[3] =  CRC8(dataBuffer, 3, CRC_KEY);

    msg.addr = BQ769X0_I2C_ADDR;
    msg.buf = dataBuffer + 1;           //这里相当于&dataBuffer[1]，
    msg.flags = I2C_WR;
    msg.tx_len = 3;

    if(iic_transfermessage(&I2C_1, &msg, 1)!= 1)
    {
		LOG_E("Write Register Byte With CRC Fail");

		return false;
    }                                                       

    return true;
}

static bool BQ769X0_WriteRegisterWordWithCRC(uint8_t Register, uint16_t data)
{
    uint8_t dataBuffer[6];
    struct IIC_MessagetypeDef msg = {0};

    dataBuffer[0] = BQ769X0_I2C_ADDR << 1;
    dataBuffer[1] = Register;
    dataBuffer[2] = LOW_BYTE(data);
    dataBuffer[3] = CRC8(dataBuffer,3,CRC_KEY);
    dataBuffer[4] = HIGH_BYTE(data);
    dataBuffer[5] = CRC8(dataBuffer+4,1,CRC_KEY);

    msg.addr = BQ769X0_I2C_ADDR;
    msg.buf = dataBuffer + 1;
    msg.flags = I2C_WR;
    msg.tx_len = 5;

    if(iic_transfermessage(&I2C_1, &msg, 1) != 1)
    {
		LOG_E("Write Register Word With CRC Fail");

		return false;
    }

    return true;
}

static bool BQ769X0_WriteBlockWithCRC(uint8_t startAddress, uint8_t *buffer, uint8_t length)
{
    uint8_t index;
    uint8_t dataBuffer[32]={0}, *pointer;
    struct IIC_MessagetypeDef msg = {0};

    pointer = dataBuffer;
    *pointer++ = BQ769X0_I2C_ADDR << 1;
    *pointer++ = startAddress;
    *pointer++ = *buffer;
    *pointer = CRC8(dataBuffer,3,CRC_KEY);

    for(index = 1; index < length; index++)
    {
        pointer++ ;
        buffer++;
        *pointer = *buffer;
        *(pointer + 1) = CRC8(pointer,1,CRC_KEY);
        pointer ++;
    }

    msg.addr = BQ769X0_I2C_ADDR;
    msg.buf = dataBuffer + 1;
    msg.flags = I2C_WR;
    msg.tx_len = length * 2 + 1;
    if(iic_transfermessage(&I2C_1, &msg, 1) != 1)
    {
		LOG_E("Write Block With CRC Fail");

		return false;
    }

    return true;
}

static bool BQ769X0_ReadRegisterByte(uint8_t Register, uint8_t *data)
{
    
    struct IIC_MessagetypeDef msg[2] = {0};

    msg[0].addr = BQ769X0_I2C_ADDR;
    msg[0].flags = I2C_WR; 
    msg[0].buf = &Register;
    msg[0].tx_len = 1;

    msg[1].addr = BQ769X0_I2C_ADDR;
    msg[1].flags = I2C_RD ;
    msg[1].buf = data;
    msg[1].tx_len = 1;

    if(iic_transfermessage(&I2C_1, msg, 2) != 2)
    {
		LOG_E("Read Register Byte Fail");

		return false;
    }

    return true;    
}

static bool BQ769X0_ReadRegisterByteWithCRC(uint8_t Register, uint8_t *data)
{
    uint8_t dataBuffer[2],crcInput[2], crcValue;
    struct IIC_MessagetypeDef msg[2] = {0};

    msg[0].addr = BQ769X0_I2C_ADDR;
    msg[0].flags = I2C_WR;
    msg[0].buf = &Register;
    msg[0].tx_len = 1;

    msg[1].addr = BQ769X0_I2C_ADDR;
    msg[1].flags = I2C_RD;
    msg[1].buf = dataBuffer;
    msg[1].tx_len = 2;

    if(iic_transfermessage(&I2C_1, msg, 2) != 2)
    {
		LOG_E("Read Register Byte With CRC Fail");

		return false;
    }

    crcInput[0] = (BQ769X0_I2C_ADDR << 1) + 1;
    crcInput[1] = dataBuffer[0];
    crcValue = CRC8(crcInput,2,CRC_KEY); 
    if(crcValue != dataBuffer[1]) 
    {
		LOG_E("Read Register Byte CRC Check Fail");
		return false;
    }  
    *data = dataBuffer[0];
    return true;
}

static bool BQ769X0_ReadRegisterWordWithCRC(uint8_t Register, uint16_t *data)
{
    uint8_t dataBuffer[4],crcInput[2], crcValue;
    struct IIC_MessagetypeDef msg [2]= {0};

    msg[0].addr = BQ769X0_I2C_ADDR;
    msg[0].flags = I2C_WR;
    msg[0].buf = &Register;
    msg[0].tx_len = 1;

    msg[1].addr = BQ769X0_I2C_ADDR;
    msg[1].flags = I2C_RD;
    msg[1].buf = dataBuffer;
    msg[1].tx_len = 4;

    if(iic_transfermessage(&I2C_1, msg, 2) != 2)
    {
		LOG_E("Read Register Word With CRC Fail");

		return false;
    }

    crcInput[0] = (BQ769X0_I2C_ADDR << 1) + 1;
    crcInput[1] = dataBuffer[0];  
    crcValue = CRC8(crcInput,2,CRC_KEY);
    if(crcValue != dataBuffer[1])
    {
		LOG_E("Read Register Word CRC Check Fail");
		return false;
    }  

    crcValue = CRC8(dataBuffer + 2,1,CRC_KEY);
    if(crcValue != dataBuffer[3])
    {
		LOG_E("Read Register Word CRC Check Fail");
		return false;
    }
    *data = (dataBuffer[2] << 8) | dataBuffer[0];
    return true;
}

static bool BQ769X0_ReadBlockWithCRC(uint8_t Register, uint8_t *buffer, uint8_t length)
{
    uint8_t index,crcInput[2], crcValue; 
    uint8_t dataBuffer[32]={0}; 
    uint8_t *pointer = dataBuffer ;
    struct IIC_MessagetypeDef msg[2] = {0};

    msg[0].addr = BQ769X0_I2C_ADDR;
    msg[0].flags = I2C_WR;
    msg[0].buf = &Register;
    msg[0].tx_len = 1;

    msg[1].addr = BQ769X0_I2C_ADDR;
    msg[1].flags = I2C_RD;
    msg[1].buf = dataBuffer;
    msg[1].tx_len = length * 2;

    if(iic_transfermessage(&I2C_1, msg, 2) != 2)
    {
		LOG_E("Read Block With CRC Fail");

		return false;
    }
    crcInput[0] = (BQ769X0_I2C_ADDR << 1) + 1;
    crcInput[1] = pointer[0];
    crcValue = CRC8(crcInput,2,CRC_KEY);
    pointer++;
    if(crcValue != *pointer)
    {
		LOG_E("Read Block CRC Check Fail");
		return false;
    }
    else
    {
        *buffer = *(pointer - 1);       //这一步相当于把第一个校验完的数据存入要读出来数据的BUFFER中
    }
    
    for(index = 1; index < length; index++)
    {
        pointer++;
        crcValue = CRC8(pointer,1,CRC_KEY);
        pointer++;
        buffer++;
        if(crcValue != *pointer)
        {
			LOG_E("Read Block CRC Check Fail");
			return false;
        }
        else
        {
            *buffer = *(pointer - 1);
        }    
        
    }
    return true;
}

/**********************************ADC采集数据************************** */
//电芯电压采集
void BQ769X0_UpdateCellVolt(void)
{
    uint16_t CellVolt = 0;
    uint8_t index = 0;
    uint8_t *pointer = NULL;
    int32_t iCellvolt = 0;
    
    if(BQ769X0_ReadBlockWithCRC(VC1_HI_BYTE, &Registers.VCell1.VCell1Byte.VC1_HI, BQ769X0_CELL_MAX << 1) != true)
    {
		LOG_E("Update Cell Voltage Fail");
    }
    
    pointer = &Registers.VCell1.VCell1Byte.VC1_HI;
    for(index = 0; index < BQ769X0_CELL_MAX; index++)
    {
        CellVolt =(((uint16_t)*pointer) << 8) + *(pointer + 1);
        iCellvolt = ((int32_t)CellVolt * iGain)/1000;
        iCellvolt +=Adcoffset;
        BQ769X0_SampleData.CellVoltage[index] = (float)iCellvolt/1000.0f;
        pointer += 2;
    } 
    /* LOG_W("Cell Volt Sample Done:");
    for(index = 0; index < BQ769X0_CELL_MAX; index++)
    {
        // 使用整数模拟浮点打印，解决 %f 无效问题
        int32_t volt_mv = (int32_t)(BQ769X0_SampleData.CellVoltage[index] * 1000.0f + 0.5f); 
        LOG_W("Cell[%d]=%d.%03dV", index, volt_mv / 1000, volt_mv % 1000);
    } */
}



/* 热敏电阻温度 2s更新一次 */
void BQ769X0_UpdateTsTemp(void)
{
    uint8_t index = 0;
    uint8_t *pointer = NULL;
    uint16_t Temp = 0;
    float VTSX = 0, RTS = 0;

    if(BQ769X0_WriteRegisterByteWithCRC(SYS_CTRL1,0x18) != true) 
    {
		LOG_E("Update Tsx Temperature Fail");
    }   
    BQ769X0_DELAY(2000);

    if(BQ769X0_ReadBlockWithCRC(TS1_HI_BYTE, &(Registers.TS1.TS1Byte.TS1_HI), BQ769X0_TMEP_MAX << 1) != true)
    {
		LOG_E("Update Tsx Temperature Fail");
    }
    pointer = &Registers.TS1.TS1Byte.TS1_HI;
    for(index = 0; index < BQ769X0_TMEP_MAX; index++,pointer += 2)
    {
        Temp =(uint16_t)(*pointer << 8) | *(pointer + 1);
        VTSX = Temp * 0.000382;

        // Rts:热敏电阻阻值
		// 根据adc值算出热敏电阻阻值,单位:Ω
        RTS = (10000 * VTSX)/(3.3 - VTSX);
        
        // 根据电阻值算出对应的温度值
        BQ769X0_SampleData.Temperature[index] = TempChange(RTS);   
    }
 /*    // --- 调试打印 (确认数据后请手动注释掉) ---
    LOG_W("[NTC] Temp Sample Done:");
    for(index = 0; index < BQ769X0_TMEP_MAX; index++)
    {
        // 整数模拟浮点: 1位小数
        int32_t temp_x10 = (int32_t)(BQ769X0_SampleData.Temperature[index] * 10.0f + 0.5f);
        LOG_W("  NTC[%d]=%d.%dC", index, temp_x10 / 10, temp_x10 % 10);
    } */
}


/* 获取ic内部温度,2s更新一次,未测试好 */
void BQ769X0_UpdateDieTemp(void)
{ 
   float die_temp = 0;
   float VTSX = 0;
   uint16_t die_temp_adc = 0;
   
   
    if(BQ769X0_WriteRegisterByteWithCRC(SYS_CTRL1,0x10) != true) 
    {
		LOG_E("Update DIE Temperature Fail");
    }   
    BQ769X0_DELAY(2000);
    if(BQ769X0_ReadRegisterWordWithCRC(TS1_HI_BYTE, &Registers.TS1.TS1Word) != true)
    {
		LOG_E("Update DIE Temperature Fail");
    }
    die_temp_adc = (Registers.TS1.TS1Byte.TS1_HI << 8) | Registers.TS1.TS1Byte.TS1_LO;
    VTSX = die_temp_adc * 0.000382f;
    die_temp = 25  -((VTSX - 1.200f) / 0.0042f);

    BQ769X0_SampleData.DieTemperature = die_temp;
}

/* 更新总电流 250ms更新一次 */
void BQ769X0_UpdateCurrent(void)
{
    int16_t adc_val = 0;
    float iCurrent = 0;

    if(BQ769X0_ReadRegisterWordWithCRC(CC_HI_BYTE, &Registers.CC.CCWord) != true)
    {
		LOG_E("Update Current Fail");
    }
    adc_val = (Registers.CC.CCByte.CC_HI << 8)| Registers.CC.CCByte.CC_LO;

    //BQ769X0_INFO("current = %d", temp);
	/*CC Reading (in μV) = [16-bit 2’s Complement Value] × (8.44 μV/LSB) */
	if(adc_val & 0x8000)
	{
		adc_val = -((~adc_val + 1) & 0xFFFF);  //这是通用的补码公式不论多少位都通用。也可直接强制转换，编译器会实现
	}
	
    //单位为：A
    iCurrent = (((float)adc_val * 8.44) / RsnsValue) *0.000001f;

    BQ769X0_SampleData.BatteryCurrent = iCurrent;

   /*   // --- 调试打印 (确认数据后请手动注释掉) ---
    
        // 整数模拟浮点: 3位小数，处理负数
        int32_t curr_ma = (int32_t)(iCurrent * 1000.0f + 0.5f);
        if(curr_ma < 0) 
        {
            LOG_W("[CUR] Current=-%d.%03dA", (-curr_ma)/1000, (-curr_ma)%1000);
        } 
        else 
        {
            LOG_W("[CUR] Current=%d.%03dA", curr_ma/1000, curr_ma%1000);
        } */
}

/* 更新总电压 250ms更新一次 */
void BQ769X0_UpadteBatVolt(void)
{ 
    uint16_t bat_volt = 0;

   // float iVolt = 0;

    if(BQ769X0_ReadRegisterWordWithCRC(BAT_HI_BYTE, &Registers.VBat.VBatWord) != true)
    {
		LOG_E("Update Battery Voltage Fail");
    }
    bat_volt = (Registers.VBat.VBatByte.BAT_HI << 8) | Registers.VBat.VBatByte.BAT_LO;
    //iVolt = ((4 * Gain * bat_volt) + (BQ769X0_CELL_MAX * Adcoffset))/1000.0f;

    BQ769X0_SampleData.BatteryVoltage = 4 * Gain * bat_volt;
    BQ769X0_SampleData.BatteryVoltage += BQ769X0_CELL_MAX * Adcoffset;
    BQ769X0_SampleData.BatteryVoltage /= 1000.0;

 /*     // 整数模拟浮点: 3位小数
        int32_t volt_mv = (int32_t)(BQ769X0_SampleData.BatteryVoltage * 1000.0f + 0.5f);
        LOG_W("[BAT] Pack Volt=%d.%03dV", volt_mv/1000, volt_mv%1000); */
}


//报警回调处理函数

static void BQ769X0_AlertyHandler(void)
{
   uint8_t wrtie_val = 0, reg_val = 0;
    BQ769X0_ReadRegisterByteWithCRC(SYS_STAT, &reg_val);

    if(reg_val & SYS_STAT_OCD_BIT)
    {
       wrtie_val |= SYS_STAT_OCD_BIT;  
       if(AlertOps.ocd != NULL)  AlertOps.ocd();
    }
    if(reg_val & SYS_STAT_SCD_BIT)
    {
       wrtie_val |= SYS_STAT_SCD_BIT;  
       if(AlertOps.scd != NULL)  AlertOps.scd();
    }
    if(reg_val & SYS_STAT_OV_BIT)
    {
       wrtie_val |= SYS_STAT_OV_BIT;  
       if(AlertOps.ov != NULL)  AlertOps.ov();
    }
    if(reg_val & SYS_STAT_UV_BIT)
    {
       wrtie_val |= SYS_STAT_UV_BIT;
       if(AlertOps.uv != NULL)  AlertOps.uv();
    }

    if(reg_val & SYS_STAT_OVRD_BIT)
    {
       wrtie_val |= SYS_STAT_OVRD_BIT;  
       if(AlertOps.ovrd != NULL)  AlertOps.ovrd();
    }
    if(reg_val & SYS_STAT_DEVICE_BIT)
    {
       wrtie_val |= SYS_STAT_DEVICE_BIT;
       if(AlertOps.device != NULL)  AlertOps.device();
    }
    if(reg_val & SYS_STAT_CC_BIT)
    {
       wrtie_val |= SYS_STAT_CC_BIT;  
       if(AlertOps.cc != NULL)  AlertOps.cc();
    }

    BQ769X0_WriteRegisterByteWithCRC(SYS_STAT, wrtie_val);
}

/*获取gain/adcoffset*/
static void  BQ769X0_GetADCGainOffset(void)
{
    BQ769X0_ReadRegisterByteWithCRC(ADCGAIN1, &Registers.ADCGain1.ADCGain1Byte);
    BQ769X0_ReadRegisterByteWithCRC(ADCGAIN2, &Registers.ADCGain2.ADCGain2Byte);
    BQ769X0_ReadRegisterByteWithCRC(ADCOFFSET, &Registers.ADCOffset);  
    //Gain的单位为mv，iGain的单位为uv
    Gain = (ADCGAIN_BASE + ((Registers.ADCGain1.ADCGain1Byte & 0x0c) << 1)+ ((Registers.ADCGain2.ADCGain2Byte & 0xe0) >> 5))/1000.0;
    iGain = ADCGAIN_BASE + ((Registers.ADCGain1.ADCGain1Byte & 0x0c) << 1)+ ((Registers.ADCGain2.ADCGain2Byte & 0xe0) >> 5);

   /*Adcoffset = int8_t(Registers.ADCOffset);这样强转也可以，编译器自动处理*/ 

    if(Registers.ADCOffset <= 0x7f)
    {
        Adcoffset = Registers.ADCOffset;
    }
    else
    {
        Adcoffset = Registers.ADCOffset - 256;
    }
}

// 检测是否接了负载
// 只有在没使能充电的情况下且CHG引脚电压大于0.7V才会检测到负载
bool BQ769X0_LoadDetect(void)
{
	BQ769X0_ReadRegisterWordWithCRC(SYS_CTRL1, (uint16_t *)&Registers.SysCtrl1.SysCtrl1Byte);
	if (Registers.SysCtrl2.SysCtrl2Bit.CHG_ON == 0) // 不在充电状态下
	{
		if (Registers.SysCtrl1.SysCtrl1Bit.LOAD_PRESENT)
		{
			return true;
		}
	}
	return false;
}


// 进入低功率模式
void BQ769X0_EntryShip(void)
{
	BQ769X0_WriteRegisterByteWithCRC(SYS_CTRL1, 0x00);
	BQ769X0_WriteRegisterByteWithCRC(SYS_CTRL1, 0x01);
	BQ769X0_WriteRegisterByteWithCRC(SYS_CTRL1, 0x02);
}

//唤醒BQ芯片
void BQ769X0_Wakeup(void)
{
    //输出模式唤醒芯片
    Q769X0_TS1_WACKUP_SetOutMode();
    HAL_GPIO_WritePin(BQ76920_TS1_WACKUP_GPIO_Port,BQ76920_TS1_WACKUP_Pin, GPIO_PIN_SET);
    BQ769X0_DELAY(1000);

     HAL_GPIO_WritePin(BQ76920_TS1_WACKUP_GPIO_Port,BQ76920_TS1_WACKUP_Pin, GPIO_PIN_RESET);
    //输入模式，温度采样
    Q769X0_TS1_WACKUP_SetInMode();
    BQ769X0_DELAY(1000);
}

// 控制充放电开关,这个函数的作用不论是充电还是放电，通过以下逻辑就能实现开关，而不要分情况来写，这样逻辑简单。
void BQ769X0_ControlDSGOrCHG(BQ769X0_ControlTypedef ControlType, BQ769X0_StateTypedef NewState)
{
    if(NewState == BQ_STATE_ENABLE)
    {
        Registers.SysCtrl2.SysCtrl2Byte |= ControlType;
    }
    else
    {
        Registers.SysCtrl2.SysCtrl2Byte &= ~ControlType;
    }
    BQ769X0_WriteRegisterByteWithCRC(SYS_CTRL2, Registers.SysCtrl2.SysCtrl2Byte);
}

//控制均衡，对应的电芯的寄存器位相应的写入1，开启均衡
// 设置某个电芯均衡状态，可以位与多节，支持BQ769X0系列(相邻单元不能同时均衡)
void BQ769X0_CellBalanceControl(BQ769X0_CellIndexTypedef CellIndex, BQ769X0_StateTypedef NewState)
{
    uint8_t CELL_VAL_VALUE[3] = {0};

    if(NewState == BQ_STATE_ENABLE)
    {
        CELL_VAL_VALUE[0] = CellIndex & 0x1f;
        CELL_VAL_VALUE[1] = (CellIndex >> 5) & 0x1f;
        CELL_VAL_VALUE[2] = (CellIndex >> 10) & 0x1f;

    }
    else
    {
        CELL_VAL_VALUE[0] = ~(CellIndex & 0x1f);
        CELL_VAL_VALUE[1] = ~((CellIndex >> 5) & 0x1f);
        CELL_VAL_VALUE[2] = ~((CellIndex >> 10) & 0x1f);
    }
    BQ769X0_WriteBlockWithCRC(CELLBAL1, CELL_VAL_VALUE,3);
}

static void BQ769X0_Configuration(void)
{ 
    uint8_t ReadBuffer[8]={0};

    //开启ADC同时设置外部热敏电阻测温
    Registers.SysCtrl1.SysCtrl1Byte = 0x18;
    // 使能电流连续采样，关闭充放电MOS
	Registers.SysCtrl2.SysCtrl2Byte = 0x40;
    // 配置CC_CFG,说明书要求在初始化时应配置为0X19以获得更好的性能
	Registers.CCCfg = 0x19;

    BQ769X0_WriteBlockWithCRC(SYS_CTRL1, &(Registers.SysCtrl1.SysCtrl1Byte), 8);
    BQ769X0_DELAY(1000);
    BQ769X0_ReadBlockWithCRC(SYS_CTRL1, ReadBuffer, 8);

    /*  // --- 新增调试打印代码 ---
    LOG_E("=== Register Check Debug ===");
    LOG_E("Addr 0x%02X: Write=0x%02X, Read=0x%02X, Match=%d", 
          SYS_CTRL1, Registers.SysCtrl1.SysCtrl1Byte, ReadBuffer[0], 
          ((ReadBuffer[0]&0x7F) == Registers.SysCtrl1.SysCtrl1Byte));
          
    LOG_E("Addr 0x%02X: Write=0x%02X, Read=0x%02X, Match=%d", 
          SYS_CTRL1+1, Registers.SysCtrl2.SysCtrl2Byte, ReadBuffer[1], 
          (ReadBuffer[1] == Registers.SysCtrl2.SysCtrl2Byte));

    LOG_E("Addr 0x%02X: Write=0x%02X, Read=0x%02X, Match=%d", 
          SYS_CTRL1+2, Registers.Protect1.Protect1Byte, ReadBuffer[2], 
          (ReadBuffer[2] == Registers.Protect1.Protect1Byte));
          
    LOG_E("Addr 0x%02X: Write=0x%02X, Read=0x%02X, Match=%d", 
          SYS_CTRL1+3, Registers.Protect2.Protect2Byte, ReadBuffer[3], 
          (ReadBuffer[3] == Registers.Protect2.Protect2Byte));

    LOG_E("Addr 0x%02X: Write=0x%02X, Read=0x%02X, Match=%d", 
          SYS_CTRL1+4, Registers.Protect3.Protect3Byte, ReadBuffer[4], 
          (ReadBuffer[4] == Registers.Protect3.Protect3Byte));

    LOG_E("Addr 0x%02X: Write=0x%02X, Read=0x%02X, Match=%d", 
          SYS_CTRL1+5, Registers.OVTrip, ReadBuffer[5], 
          (ReadBuffer[5] == Registers.OVTrip));

    LOG_E("Addr 0x%02X: Write=0x%02X, Read=0x%02X, Match=%d", 
          SYS_CTRL1+6, Registers.UVTrip, ReadBuffer[6], 
          (ReadBuffer[6] == Registers.UVTrip));

    LOG_E("Addr 0x%02X: Write=0x%02X, Read=0x%02X, Match=%d", 
          SYS_CTRL1+7, Registers.CCCfg, ReadBuffer[7], 
          (ReadBuffer[7] == Registers.CCCfg));
    // ------------------------- */

    	// 去掉BUFF[0]的最高位,防止因为接上了负载使负载检测置位而没通过校验
	if( (ReadBuffer[0]&0X7F) != Registers.SysCtrl1.SysCtrl1Byte
	|| ReadBuffer[1] != Registers.SysCtrl2.SysCtrl2Byte
	|| ReadBuffer[2] != Registers.Protect1.Protect1Byte
	|| ReadBuffer[3] != Registers.Protect2.Protect2Byte
	|| ReadBuffer[4] != Registers.Protect3.Protect3Byte
	|| ReadBuffer[5] != Registers.OVTrip
	|| ReadBuffer[6] != Registers.UVTrip
	|| ReadBuffer[7] != Registers.CCCfg)
	{
		LOG_E("BQ769X0 config register fail,Please reset BMS board");

		while(1);
	}
}



void BQ769X0_Init(BQ769X0_InitDataTypedef *InitData)
{ 
    

    /* uint8_t test_val = 0x19; // 尝试写入 0x01
    uint8_t read_back = 0; */
    // 进入睡眠再唤醒相当于复位一次BQ芯片
     BQ769X0_EntryShip();
     BQ769X0_DELAY(500);
     BQ769X0_Wakeup();

    AlertOps = InitData->AlertOps;

    BQ769X0_GetADCGainOffset();


    /* BQ769X0_WriteRegisterByte(0x0B, test_val);
    BQ769X0_DELAY(20);
    BQ769X0_ReadRegisterByte(0x0B, &read_back);
    LOG_E("Test Write: 0x%02X, Read Back: 0x%02X", test_val, read_back); */
    //配置各个寄存器的值

    

    Registers.Protect1.Protect1Bit.SCD_THRESH = InitData->ConfigData.SCDThreshold;
    Registers.Protect1.Protect1Bit.SCD_DELAY = InitData->ConfigData.SCDDelay;
    Registers.Protect2.Protect2Bit.OCD_THRESH = InitData->ConfigData.OCDThreshold;
    Registers.Protect2.Protect2Bit.OCD_DELAY = InitData->ConfigData.OCDDelay;
    Registers.Protect3.Protect3Bit.OV_DELAY = InitData->ConfigData.OVDelay;
    Registers.Protect3.Protect3Bit.UV_DELAY = InitData->ConfigData.UVDelay;

    //Registers.OVTrip = (uint8_t)(((uint16_t)((InitData->ConfigData.OVPThreshold - Adcoffset)/Gain) >> 4) & 0xff) ;
    //Registers.UVTrip = (uint8_t)(((uint16_t)((InitData->ConfigData.UVPThreshold - Adcoffset)/Gain) >> 4) & 0xff) ;

     // BQ阈值寄存器内部比较是14位的，但我们真实写入的值是“10-XXXX-XXXX–1000”中间x的数据，所以下面计算出14位数据后需要得到中间8位再写入
    // 减去OV_THRESH_BASE就是得到中间8位，，在数据手册7.3.1.2.1章节有讲解
    Registers.OVTrip = (uint8_t)((((uint16_t)((InitData->ConfigData.OVPThreshold - Adcoffset)/Gain/* + 0.5*/) - OV_THRESH_BASE) >> 4) & 0xFF);
    // BQ阈值寄存器内部比较是14位的，但我们真实写入的值是“01-XXXX-XXXX–0000”中间x的数据，所以下面计算出14位数据后需要得到中间8位再写入
    // 减去UV_THRESH_BASE就是得到中间8位，在数据手册7.3.1.2.1章节有讲解
    Registers.UVTrip = (uint8_t)((((uint16_t)((InitData->ConfigData.UVPThreshold - Adcoffset)/Gain/* + 0.5*/) - UV_THRESH_BASE) >> 4) & 0xFF); 
    BQ769X0_Configuration();
    

    LOG_I("BQ769X0 Initialize successful!");
}

