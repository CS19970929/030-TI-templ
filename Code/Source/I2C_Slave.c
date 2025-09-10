#include "main.h"

UINT8 g_u8DataRxCnt;
UINT8 g_u8StopRxCnt;
UINT8 g_u8I2cTxD0Buff[I2C_BUFF_D0_SIZE];
UINT8 g_u8I2cTxD1Buff[I2C_BUFF_D1_SIZE];
UINT8 *g_pI2cTx;

UINT8 g_u8SR1 = 0;
UINT8 g_u8SR3 = 0;
;

void I2cSlaver_DataInit(void);

void Init_I2CSlaver(void)
{
}

void I2cSlaver_DataInit(void)
{
    UINT8 i = 0;

    g_u8DataRxCnt = 0;
    g_u8StopRxCnt = 0;
    for (i = 0; i < I2C_BUFF_D0_SIZE; i++)
    {
        g_u8I2cTxD0Buff[i] = 0;
    }
    g_u8I2cTxD0Buff[0] = I2C_CMD_D0;

    for (i = 0; i < I2C_BUFF_D1_SIZE; i++)
    {
        g_u8I2cTxD1Buff[i] = 0;
    }
    g_u8I2cTxD1Buff[0] = I2C_CMD_D1;

    g_pI2cTx = g_u8I2cTxD0Buff;
}

/******************************************************
*函数名称:CheckSumSum16
*输   入:buf 要校验的数据
        len 校验数据的长
*输   出:校验和
*功   能:校验和-16
*******************************************************/
UINT16 CheckSum16(UINT8 *buf, UINT8 len)
{
    UINT8 i = 0;
    UINT16 Sum = 0;

    for (i = 0; i < len; i++)
    {
        Sum += *buf++;
    }

    return Sum;
}

void I2cSlaver_DataPacketD0(void)
{
    UINT8 i = 0;
    UINT16 t_u16Temp = 0;

    i = 0;
    g_u8I2cTxD0Buff[i++] = I2C_CMD_D0;

    // 电池总电压
    t_u16Temp = (UINT16)(g_stBq769x0_Read_AFE1.u32VBat / 10);
    g_u8I2cTxD0Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);
    g_u8I2cTxD0Buff[i++] = (UINT8)((t_u16Temp >> 8) & 0x00FF);

    // 电流
    t_u16Temp = g_stBq769x0_Read_AFE1.u16Current / 10;
    g_u8I2cTxD0Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);
    g_u8I2cTxD0Buff[i++] = (UINT8)((t_u16Temp >> 8) & 0x00FF);

    // 剩余容量
    // t_u16Temp = SOC_cal_element.C*10/3600;
    t_u16Temp = 0;
    g_u8I2cTxD0Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);
    g_u8I2cTxD0Buff[i++] = (UINT8)((t_u16Temp >> 8) & 0x00FF);

    // 标称容量
    // t_u16Temp = (UINT16)((SOC_cal_element.C1 *10 /3600)&0xFFFF);
    t_u16Temp = 0;
    g_u8I2cTxD0Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);
    g_u8I2cTxD0Buff[i++] = (UINT8)((t_u16Temp >> 8) & 0x00FF);

    // 循环次数
    t_u16Temp = 0;
    g_u8I2cTxD0Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);
    g_u8I2cTxD0Buff[i++] = (UINT8)((t_u16Temp >> 8) & 0x00FF);

    // 生产日期
    t_u16Temp = 0x6345;
    g_u8I2cTxD0Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);
    g_u8I2cTxD0Buff[i++] = (UINT8)((t_u16Temp >> 8) & 0x00FF);

    // 均衡状态
    t_u16Temp = 0x7335;
    g_u8I2cTxD0Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);
    g_u8I2cTxD0Buff[i++] = (UINT8)((t_u16Temp >> 8) & 0x00FF);

    // 均衡状态-高
    t_u16Temp = 0x8365;
    g_u8I2cTxD0Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);
    g_u8I2cTxD0Buff[i++] = (UINT8)((t_u16Temp >> 8) & 0x00FF);

    // 保护状态
    t_u16Temp = 0x9345;
    g_u8I2cTxD0Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);
    g_u8I2cTxD0Buff[i++] = (UINT8)((t_u16Temp >> 8) & 0x00FF);

    // 软件版本
    t_u16Temp = 0xA31B;
    g_u8I2cTxD0Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);

    // 剩余容量百分比
    t_u16Temp = 0xB31D;
    g_u8I2cTxD0Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);

    // FET控制状态
    t_u16Temp = 0xC335;
    g_u8I2cTxD0Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);

    // 电池串数
    t_u16Temp = 8;
    g_u8I2cTxD0Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);

    // NTC数量
    t_u16Temp = 3;
    g_u8I2cTxD0Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);

    // NTC温度1
    t_u16Temp = 0xF3AC;
    g_u8I2cTxD0Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);

    // NTC温度2
    t_u16Temp = 0x03EF;
    g_u8I2cTxD0Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);

    // NTC温度3
    t_u16Temp = 0x13FD;
    g_u8I2cTxD0Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);

    // 校验和
    t_u16Temp = CheckSum16(g_u8I2cTxD0Buff, i);
    g_u8I2cTxD0Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);
    g_u8I2cTxD0Buff[i++] = (UINT8)((t_u16Temp >> 8) & 0x00FF);
}

void I2cSlaver_DataPacketD1(void)
{
    UINT8 i = 0;
    UINT16 t_u16Temp = 0;

    i = 0;
    g_u8I2cTxD1Buff[i++] = I2C_CMD_D1;

    // Cell1电压
    t_u16Temp = g_stBq769x0_Read_AFE1.u16VCell[0];
    g_u8I2cTxD1Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);
    g_u8I2cTxD1Buff[i++] = (UINT8)((t_u16Temp >> 8) & 0x00FF);

    // Cell2电压
    t_u16Temp = g_stBq769x0_Read_AFE1.u16VCell[1];
    g_u8I2cTxD1Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);
    g_u8I2cTxD1Buff[i++] = (UINT8)((t_u16Temp >> 8) & 0x00FF);

    // Cell3电压
    t_u16Temp = g_stBq769x0_Read_AFE1.u16VCell[2];
    g_u8I2cTxD1Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);
    g_u8I2cTxD1Buff[i++] = (UINT8)((t_u16Temp >> 8) & 0x00FF);

    // Cell4电压
    t_u16Temp = g_stBq769x0_Read_AFE1.u16VCell[3];
    g_u8I2cTxD1Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);
    g_u8I2cTxD1Buff[i++] = (UINT8)((t_u16Temp >> 8) & 0x00FF);

    // Cell5电压
    t_u16Temp = g_stBq769x0_Read_AFE1.u16VCell[4];
    g_u8I2cTxD1Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);
    g_u8I2cTxD1Buff[i++] = (UINT8)((t_u16Temp >> 8) & 0x00FF);

    // Cell6电压
    t_u16Temp = g_stBq769x0_Read_AFE1.u16VCell[5];
    g_u8I2cTxD1Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);
    g_u8I2cTxD1Buff[i++] = (UINT8)((t_u16Temp >> 8) & 0x00FF);

    // Cell7电压
    t_u16Temp = g_stBq769x0_Read_AFE1.u16VCell[6];
    g_u8I2cTxD1Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);
    g_u8I2cTxD1Buff[i++] = (UINT8)((t_u16Temp >> 8) & 0x00FF);

    // Cell8电压
    t_u16Temp = g_stBq769x0_Read_AFE1.u16VCell[7];
    g_u8I2cTxD1Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);
    g_u8I2cTxD1Buff[i++] = (UINT8)((t_u16Temp >> 8) & 0x00FF);

    // Cell9电压
    t_u16Temp = g_stBq769x0_Read_AFE1.u16VCell[8];
    g_u8I2cTxD1Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);
    g_u8I2cTxD1Buff[i++] = (UINT8)((t_u16Temp >> 8) & 0x00FF);

    // Cell10电压
    t_u16Temp = g_stBq769x0_Read_AFE1.u16VCell[9];
    g_u8I2cTxD1Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);
    g_u8I2cTxD1Buff[i++] = (UINT8)((t_u16Temp >> 8) & 0x00FF);

    // Cell11电压
    t_u16Temp = g_stBq769x0_Read_AFE1.u16VCell[10];
    g_u8I2cTxD1Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);
    g_u8I2cTxD1Buff[i++] = (UINT8)((t_u16Temp >> 8) & 0x00FF);

    // Cell12电压
    t_u16Temp = g_stBq769x0_Read_AFE1.u16VCell[11];
    g_u8I2cTxD1Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);
    g_u8I2cTxD1Buff[i++] = (UINT8)((t_u16Temp >> 8) & 0x00FF);

    // Cell13电压
    t_u16Temp = g_stBq769x0_Read_AFE1.u16VCell[12];
    g_u8I2cTxD1Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);
    g_u8I2cTxD1Buff[i++] = (UINT8)((t_u16Temp >> 8) & 0x00FF);

    // Cell14电压
    t_u16Temp = g_stBq769x0_Read_AFE1.u16VCell[13];
    g_u8I2cTxD1Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);
    g_u8I2cTxD1Buff[i++] = (UINT8)((t_u16Temp >> 8) & 0x00FF);

    // Cell15电压
    t_u16Temp = g_stBq769x0_Read_AFE1.u16VCell[14];
    g_u8I2cTxD1Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);
    g_u8I2cTxD1Buff[i++] = (UINT8)((t_u16Temp >> 8) & 0x00FF);

    // Cell16电压
    // t_u16Temp = g_stBq769x0_Read_AFE1.u16VCell[15];//TODO 15
    t_u16Temp = 0;
    g_u8I2cTxD1Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);
    g_u8I2cTxD1Buff[i++] = (UINT8)((t_u16Temp >> 8) & 0x00FF);

    // 校验和
    t_u16Temp = CheckSum16(g_u8I2cTxD1Buff, i);
    g_u8I2cTxD1Buff[i++] = (UINT8)(t_u16Temp & 0x00FF);
    g_u8I2cTxD1Buff[i++] = (UINT8)((t_u16Temp >> 8) & 0x00FF);
}

// 从机没有时钟延展：NOSTRETCH，clock stretching通过将SCL线拉低来暂停一个传输.直到释放SCL线为高电平,传输才继续进行
// 看SR1寄存器得出的结论，在CR1寄存器第7位
// 已经看清其机制了，其默认是slave模式。怎么切换看下面这段文字
// By default, it operates in slave mode. The interface automatically switches from slave to master, after it generates a START condition
// and from master to slave, if an arbitration loss or a STOP generation occurs, allowing Multi-Master capability.

// 关键寄存器是I2C1->SR3的Bit2 TRA: Transmitter/Receiver
// 0: Data bytes received
// 1: Data bytes transmitted
// This bit is set depending on R/W bit of address byte, at the end of total address phase.
// It is also cleared by hardware after detection of Stop condition (STOPF=1), repeated Start condition,
// loss of bus arbitration (ARLO=1), or when PE=0.
// 该寄存器的控制如下：
// In 7-bit addressing mode,
// – To enter Transmitter mode, a master sends the slave address with LSB reset.
// – To enter Receiver mode, a master sends the slave address with LSB set.
// It is also cleared by hardware after detection of Stop condition (STOPF=1), repeated Start condition,
// loss of bus arbitration (ARLO=1), or when PE=0.
// 再次纠正，这个是master模式下的用法，但是在salver模式下######是反过来的######，划重点，规格书没说，实验得出的结论
// 还有一个感悟，IAR编译器本身观察寄存器的值是错误的，要想正确得到寄存器的值，可用赋值，LiveWatch观察

// ACK为SDA低电平，NACK为高电平
// 过程：
// A：if(I2C1->SR1&0x02)
// 上位机先发送Start信号，然后接着一个Adress(+Wr0)，TRA被置为0(received mode)，先匹配这个：if(I2C1->SR1&0x02)，进入填数据函数，回一个ACK

// B：else if((I2C1->SR1&0x40)&&(0 == (I2C1->SR3&0x04)))
// 然后读到CommandCode，进入这个else if((I2C1->SR1&0x40)&&(0 == (I2C1->SR3&0x04)))，读取0xD0或者0xD1，这个时候，把发送指针指向相应的数组，回一个ACK

// C：if(I2C1->SR1&0x02)
// 然后上位机再发Sr，起始信号，TRA位被硬件清除。然后发Adress(+Rr1)，再次进入if(I2C1->SR1&0x02)，填数据函数，TRA被置位1(Transmitter mode)，回一个ACK

// D：else if((I2C1->SR1&0x84)&&(I2C1->SR3&0x04))
// 进入else if((I2C1->SR1&0x84)&&(I2C1->SR3&0x04))，开始不停发数据，上位机接收够相应数量的字节后，发出NACK，不再发数据。再发出一个Stop信号，TRA位被硬件清除，结束发送数据模式

// 汇总:这个东西是我目前的感悟，关于TRA这个位，还是需要权威解释，如果没用，以目前的实验结果为主或者再次试验再写代码。
void I2C_Salve_Deal(void)
{
}
