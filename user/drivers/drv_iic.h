#ifndef __DRV_IIC_H__
#define __DRV_IIC_H__

#include <stdio.h>
#include "stm32f1xx_hal.h"
#include <rtthread.h>
#include "rthw.h"

#define I2C_DEBUG_LEVEL 3

#if I2C_DEBUG_LEVEL == 0
#define I2C_INFO(fmt, arg...) 
#define I2C_WARNING(fmt, arg...)
#define I2C_ERROR(fmt, arg...)

#elif I2C_DEBUG_LEVEL == 1
#define I2C_INFO(fmt, arg...)   		rt_kprintf("<<-I2C-INFO->> "fmt"\r\n",##arg)
#define I2C_WARNING(fmt, arg...)
#define I2C_ERROR(fmt, arg...)

#elif I2C_DEBUG_LEVEL == 2
#define I2C_INFO(fmt, arg...)
#define I2C_WARNING(fmt, arg...)		rt_kprintf("<<-I2C-WARNING->> "fmt"\r\n",##arg)
#define I2C_ERROR(fmt, arg...)

#elif I2C_DEBUG_LEVEL == 3
#define I2C_INFO(fmt, arg...)           //rt_kprintf("<<-I2C-INFO->> "fmt"\r\n",##arg)    
#define I2C_WARNING(fmt, arg...)        rt_kprintf("<<-I2C-WARNING->> "fmt"\r\n",##arg)
#define I2C_ERROR(fmt, arg...)	 		rt_kprintf("<<-I2C-ERROR->> "fmt"\r\n",##arg)
#endif


#define I2C_WR              0x00       		/* 写标志 */
#define I2C_RD              (1 << 0)      	/* 读标志 */
#define I2C_ADDR_10BIT      (1 << 1)      	/* 10 位地址模式 */
#define I2C_NO_START        (1 << 2)      	/* 无开始条件 */
#define I2C_IGNORE_NACK     (1 << 3)      	/* 忽视 NACK */
#define I2C_NO_READ_ACK     (1 << 4)      	/* 读的时候不发送 ACK */
#define I2C_NO_STOP         (1 << 5) 		/* 传输完不发送停止信号 */

// 下面两种标志位我自己添加的
#define I2C_CONTROL_BYTE    (1 << 6) 		/* 每发送一个数据之前需要发送一个控制字节用来表示之后的数据字节是命令还是数据(有的场景会用到比如：ssd1306驱动屏) */
#define I2C_SAME_BYTE       (1 << 7) 		/* 连续发送msg.tLen个msg.sByte数据字节,发送相同字节会用到避免循环调用传输函数 */

struct IIC_BUStypeDef
{
    GPIO_TypeDef *gpio_port;
    uint16_t scl_pin;
    uint16_t sda_pin;
    uint16_t retries;    //发地址无响应重复次数
    void(*udelay)(uint32_t us);
    void(*lockinit)(void);
    void(*lock)(void);
    void(*unlock)(void);
};

struct IIC_MessagetypeDef
{
    uint16_t addr;           //从设备地址，基于BQ76920的I2C地址，只使用7位的IIC地址
    uint8_t *buf;           //数据缓冲区
    uint16_t tx_len;        //发送数据长度
    uint16_t rx_len;        //返回成功传输的数据长度
    uint8_t flags;
    uint8_t   cByte;	 	// I2C_CONTROL_BYTE
	uint8_t   sByte;	 	// I2C_SAME_BYTE
};

extern struct IIC_BUStypeDef I2C_1;



void iic_BUS_init(void);
uint32_t iic_transfermessage(struct IIC_BUStypeDef *bus,struct IIC_MessagetypeDef msgs[],uint32_t num);


#endif










