/*梳理一下逻辑，需要写的函数，然后根据函数反推变量定义，IIC初始化引脚配置，
start,stop,ack,RW写，最后信息发送*/
#include "drv_iic.h"
#include "main.h"

// 使用互斥锁会被高优先级任务抢占
// 偶发性的导致i2c信号传输一半的时候就跑去做其他的
// 最后导致读写i2c数据不对进而BQ芯片驱动的CRC通不过
//static struct rt_mutex mutex1 ={0};

static rt_uint32_t level;

/*I2C1_Lock()作用：进入临界区前，禁用全局中断，防止在操作 I2C 期间被其他中断（如另一个任务或 ISR）打断。
 将当前的中断状态保存到变量 level 中。
I2C1_Unlock()  作用：退出临界区后，恢复之前的中断状态（通常是重新使能中断）。 使用之前保存的 level 值
来恢复中断使能状态，确保不会错误地开启/关闭不该动的中断。
这种“关中断 + 开中断”的配对使用是 RTOS 中实现轻量级互斥（无锁同步）的常见手段，适用于短时间、高优先级的临界区保护。*/
static void I2C1_LockInit(void)
{
    //rt_mutex_init(&mutex1, "i2c1_lock", RT_IPC_FLAG_FIFO);
}

static void I2C1_Lock(void)
{
    //rt_mutex_take(&mutex1, RT_WAITING_FOREVER);

    level = rt_hw_interrupt_disable();
}

static void I2C1_Unlock(void)
{
    //rt_mutex_release(&mutex1);

    rt_hw_interrupt_enable(level);
}

// 适用于72MHZ
static void udelay_us(uint32_t us)
{
	uint16_t i = 0;
	
	while(us--)
	{
		i = 10; //自己定义
		while(i--);
	}
}


struct IIC_BUStypeDef I2C_1 = 
{
    .gpio_port = SCL_GPIO_Port,
    .scl_pin   = SCL_Pin,
    .sda_pin   = SDA_Pin,
    .retries   = 3,
    .udelay     = (void (*)(uint32_t))udelay_us,
    .lockinit  = I2C1_LockInit,
    .lock      = I2C1_Lock,
    .unlock    = I2C1_Unlock,
};

static void iic_init(struct IIC_BUStypeDef *bus)
{ 
  GPIO_InitTypeDef GPIO_InitStruct;
  GPIO_InitStruct.Pin = bus->scl_pin | bus->sda_pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

  HAL_GPIO_Init(bus->gpio_port, &GPIO_InitStruct);

  HAL_GPIO_WritePin(bus->gpio_port, bus->scl_pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(bus->gpio_port, bus->sda_pin, GPIO_PIN_SET);

  //lockinit
  if(bus->lockinit)
  {
    bus->lockinit();
  }
}

/*
我理解了为何要配置这个的原因，但是在此项目中用不到，
若是作为一个移植的规范，我也移植上
其中SDA_setoutmode,SDA_setinmode.
它是为了配合主从机等待响应的，至于原因解释如下：
*/
// stm32的IO口结构输出模式下是没有关断输入部分的肖特基触发器,数据依然会读入输入寄存器,故不用设置SDA输入模式
// 但为了保险起见最好还是写上,也为了方便将该驱动移植到其他平台或者HAL库上
static inline void SDA_setoutmode(struct IIC_BUStypeDef *bus)
{
  GPIO_InitTypeDef GPIO_InitStruct;
  GPIO_InitStruct.Pin = bus->sda_pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

  HAL_GPIO_Init(bus->gpio_port, &GPIO_InitStruct);
}

static inline void SDA_setinmode(struct IIC_BUStypeDef *bus)
{
  GPIO_InitTypeDef GPIO_InitStruct;
  GPIO_InitStruct.Pin = bus->sda_pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

  HAL_GPIO_Init(bus->gpio_port, &GPIO_InitStruct);
}

static inline uint8_t SDA_read(struct IIC_BUStypeDef *bus)
{ 
  return HAL_GPIO_ReadPin(bus->gpio_port, bus->sda_pin);
}

static inline void SDA_H(struct IIC_BUStypeDef *bus)
{ 
  HAL_GPIO_WritePin(bus->gpio_port, bus->sda_pin, GPIO_PIN_SET);
}

static inline void SDA_L(struct IIC_BUStypeDef *bus)
{ 
  HAL_GPIO_WritePin(bus->gpio_port, bus->sda_pin, GPIO_PIN_RESET);
}

static inline void SCL_H(struct IIC_BUStypeDef *bus)
{ 
  HAL_GPIO_WritePin(bus->gpio_port, bus->scl_pin, GPIO_PIN_SET);
}

static inline void SCL_L(struct IIC_BUStypeDef *bus)
{ 
  HAL_GPIO_WritePin(bus->gpio_port, bus->scl_pin, GPIO_PIN_RESET);
}

static inline void iic_start(struct IIC_BUStypeDef *bus)
{ 
   SDA_L(bus);
   bus->udelay(1);
   SCL_L(bus);
}

static inline void iic_restart(struct IIC_BUStypeDef *bus)
{ 
   SDA_H(bus);
   SCL_H(bus);
   bus->udelay(1);
   SDA_L(bus);
   bus->udelay(1);
   SCL_L(bus);
}
static inline void iic_stop(struct IIC_BUStypeDef *bus)
{ 
   SDA_L(bus);
   bus->udelay(1);
   SCL_H(bus);
   bus->udelay(1);
   SDA_H(bus);
   bus->udelay(1);
}

static inline uint8_t iic_waitack(struct IIC_BUStypeDef *bus)
{ 
  uint8_t ack;
  SDA_H(bus);
  bus->udelay(1);
  SCL_H(bus);
  SDA_setinmode(bus);
  ack = !SDA_read(bus);
  SDA_setoutmode(bus);
  I2C_INFO("%s", ack ? "ACK" : "NACK");
  SCL_L(bus);

  return ack;
}

static inline void iic_ackornack(struct IIC_BUStypeDef *bus,uint8_t ack)
{
  if(ack)
  {
    SDA_L(bus);
    bus->udelay(1);
    SCL_H(bus);
    SCL_L(bus);
  }
}

static inline uint8_t iic_writebyte(struct IIC_BUStypeDef *bus,uint8_t dat)
{ 
  uint8_t mask;
  for(mask = 0x80;mask != 0;mask >>= 1)
  { 
    SCL_L(bus);
    dat & mask ? SDA_H(bus) : SDA_L(bus);
    bus->udelay(1);
    SCL_H(bus);
  }
  SCL_L(bus);
	bus->udelay(1);

  return iic_waitack(bus);
}


static inline uint8_t iic_readbyte(struct IIC_BUStypeDef *bus)
{ 
  uint8_t  mask, data = 0;
  SDA_H(bus);
  bus->udelay(1);
  SDA_setinmode(bus);

  for(mask = 0x80;mask !=0;mask >>= 1)
  {
    SCL_H(bus);
    bus->udelay(1);
    if(SDA_read(bus))
    {
      data |= mask;
    }
    SCL_L(bus);
    bus->udelay(1);
  }
  SDA_setoutmode(bus);
  return data;
}

//发送7位IIC地址，简化使用于BQ76920，没有针对10位地址进行配置函数
static uint8_t iic_sendaddress(struct IIC_BUStypeDef *bus,uint8_t addr, uint32_t retries)
{
  uint8_t i, ret = 0;

	for (i = 0; i <= retries; i++)
	{
		ret = iic_writebyte(bus, addr);
		if (ret == 1)
		{
			I2C_INFO("response ok.");
			break;
		}
		else if (i == retries)
		{
			I2C_WARNING("no response, please check slave device.");
			break;
		}
		I2C_WARNING("no response, attempt to resend the address. number:%d.", i);
		iic_stop(bus);
		bus->udelay(1);
		iic_start(bus);
	}

	return ret;
}

static uint16_t iic_sendbytes(struct IIC_BUStypeDef *bus,struct IIC_MessagetypeDef *msg)
{ 
  uint8_t ret ;
  uint8_t *buf = msg->buf;
  uint16_t len = msg->tx_len;
  uint16_t bytes = 0;

  while(len > 0)
  {
    if (msg->flags & I2C_CONTROL_BYTE && iic_writebyte(bus, msg->cByte) == 0) // 发送控制字节
		{
			I2C_WARNING("send bytes: NACK.");
			break;
		}
    ret = msg->flags & I2C_SAME_BYTE ? iic_writebyte(bus, msg->sByte) : iic_writebyte(bus,*buf),buf++;
    if((ret > 0) || (msg->flags & I2C_IGNORE_NACK && (ret == 0)))
    {
      len--;
      bytes++;
    }
    else if(ret == 0)
    {
      I2C_WARNING("send bytes: NACK.");
      break;
    }
  }
  return bytes;
}

static uint16_t iic_recvbytes(struct IIC_BUStypeDef *bus,struct IIC_MessagetypeDef *msg)
{ 
  uint8_t val;
  uint8_t *buf = msg->buf;
  uint16_t len = msg->tx_len;
  uint16_t bytes = 0;
  while(len > 0)
  { 
    val = iic_readbyte(bus);
    *buf = val;
    bytes++;
    buf++;
    len--;
    
    I2C_INFO("recieve bytes: 0x%02x, %s",
							val, (msg->flags & I2C_NO_READ_ACK) ?
							"(No ACK/NACK)" : (len ? "ACK" : "NACK"));
    if (!(msg->flags & I2C_NO_READ_ACK))
		{
			iic_ackornack(bus, len);
		}          
  }
  return bytes;
}

static uint8_t iic_bitsendaddress(struct IIC_BUStypeDef *bus,struct IIC_MessagetypeDef *msg)
{ 
    uint8_t ret, retries, addr1, addr2;
	  uint8_t flags = msg->flags;
	  uint8_t ignore_nack = msg->flags & I2C_IGNORE_NACK;


	  retries = ignore_nack ? 0 : bus->retries;

    if (flags & I2C_ADDR_10BIT)
	  {
		    addr1 = 0xf0 | ((msg->addr >> 7) & 0x06);
		    addr2 = msg->addr & 0xff;

		    I2C_INFO("addr1: %d, addr2: %d", addr1, addr2);
        ret = iic_sendaddress(bus, addr1, retries);  
    
        if ((ret != 1) && !ignore_nack)
        {
            I2C_WARNING("NACK: sending first addr");
            return 0;
        }

        ret = iic_writebyte(bus, addr2);
        if ((ret != 1) && !ignore_nack)
        {
            I2C_WARNING("NACK: sending second addr");
            return 0;
        }
        if (flags & I2C_RD)
        {
            I2C_INFO("send repeated start condition");
            iic_restart(bus);
            addr1 |= 0x01;
            ret = iic_sendaddress(bus, addr1, retries);
            if ((ret != 1) && !ignore_nack)
            {
                I2C_ERROR("NACK: sending repeated addr");
                return 0;
            }
        }
    }
    else
    {
      /* 7-bit addr */
      addr1 = msg->addr << 1;
      if (flags & I2C_RD)
          addr1 |= 1;
      ret = iic_sendaddress(bus, addr1, retries);
      if ((ret != 1) && !ignore_nack)
          return 0;
    }

	return 1;
}

uint32_t iic_transfermessage(struct IIC_BUStypeDef *bus,struct IIC_MessagetypeDef msgs[],uint32_t num)
{ 
  struct IIC_MessagetypeDef *msg;
  uint32_t i,ret = 0;
  uint8_t ignore_nack;

  if(NULL == bus || NULL == msgs || num == 0) return ret;
  if(bus->lock) bus->lock();
  for (i = 0; i < num; i++)
  { 
    msg = &msgs[i];
    ignore_nack = msg->flags & I2C_IGNORE_NACK;
    if(!(msg->flags & I2C_NO_START))
    {
        if(i)
			  {
				  iic_restart(bus);
			  }
			  else
			  {
				  I2C_INFO("send start condition");
				  iic_start(bus);
			  }
        ret = iic_bitsendaddress(bus,msg);
      if((ret != 1) && !ignore_nack)
      {
        I2C_WARNING("receive NACK from device addr 0x%02x msg %d", msgs[i].addr, i);
				goto out;
      }
    }
    if(msg->flags & I2C_RD)
    {
      ret = iic_recvbytes(bus,msg);
      msg->rx_len = ret;

      I2C_INFO("read %d byte%s", ret, ret == 1 ? "" : "s");
    }
    else
    {
      ret = iic_sendbytes(bus,msg);
      msg->rx_len = ret;

      I2C_INFO("send %d byte%s", ret, ret == 1 ? "" : "s");
      if (msg->rx_len != msg->tx_len)
      {
        ret = 0;
				goto out;
      }
    }
  }
  ret = i;
out:
  if(!(msg->flags & I2C_NO_STOP)) 
  {
    I2C_INFO("send stop condition");
    iic_stop(bus);
  } 
  if(bus->unlock) bus->unlock();

  return ret;
}

void iic_BUS_init(void)
{   
  iic_init(&I2C_1);
}

