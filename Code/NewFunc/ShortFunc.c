#include "main.h"

const UINT16 AFE_SCV[10] = {22, 33, 44, 67, 89, 111, 133, 155, 178, 200}; // 短路保护电压，单位mv
const UINT16 AFE_SCT[10] = {50, 100, 200, 400, 400, 400, 400, 400, 400, 400};

/* 找出要写入AFE寄存器的值：参数1：当前要写的值，参数2：AFE参数列表的地址 */
UINT8 Choose_Right_Value(UINT16 cur_Value, const UINT16 *AFE_list)
{
    UINT8 i = 0;
    for (i = 0; i < 10; i++)
    {
        if (cur_Value <= AFE_list[i])
        {
            break;
        }
    }
    i = i >= 10 ? 9 : i;
    return i;
}

// 在开机，Sci(设置和复位两个函数)，三次调用便可
void InitShortCur(void)
{
    UINT16 temp = 0;

    OtherElement.u16CS_Cur_CHGmax = 2000 * OtherElement.u16Sys_CS_Res_Num / OtherElement.u16Sys_CS_Res;
    OtherElement.u16CS_Cur_DSGmax = 2000 * OtherElement.u16Sys_CS_Res_Num / OtherElement.u16Sys_CS_Res;

    /* 短路延时 */
    temp = Choose_Right_Value(OtherElement.u16CBC_DelayT / 10, AFE_SCT);
    OtherElement.u16CBC_DelayT = AFE_SCT[temp] * 10; // 修改最终设置的值，接近的那个

    Registers_AFE1.Protect1.Protect1Bit.SCD_DELAY = temp >= 3 ? 3 : temp;

    /* 短路电压 */
    temp = OtherElement.u16CBC_Cur_DSG / 10;                                   // A
    temp = temp * OtherElement.u16Sys_CS_Res / OtherElement.u16Sys_CS_Res_Num; // 当前对应多少mV
    temp = Choose_Right_Value(temp, AFE_SCV);
    // 修改最终设置的值，接近的那个
    OtherElement.u16CBC_Cur_DSG = AFE_SCV[temp] * OtherElement.u16Sys_CS_Res_Num / OtherElement.u16Sys_CS_Res;
    OtherElement.u16CBC_Cur_DSG *= 10; // 防止数据溢出。

    if (temp <= 1)
    {
        Registers_AFE1.Protect1.Protect1Bit.RSNS = 0;
        Registers_AFE1.Protect1.Protect1Bit.SCD_THRESH = temp;
    }
    else
    {
        Registers_AFE1.Protect1.Protect1Bit.RSNS = 1;
        Registers_AFE1.Protect1.Protect1Bit.SCD_THRESH = temp - 2;
    }

    // 硬件过流保护拉到最大
    Registers_AFE1.Protect2.Protect2Bit.OCD_DELAY = OCD_DELAY_1280ms;       // 8ms
    Registers_AFE1.Protect2.Protect2Bit.OCD_THRESH = OCD_THRESH_100mV_50mV; // 20A

    I2CWriteRegisterByteWithCRC(DEVICE_ADDR_AFE1, PROTECT1, Registers_AFE1.Protect1.Protect1Byte);
    I2CWriteRegisterByteWithCRC(DEVICE_ADDR_AFE1, PROTECT2, Registers_AFE1.Protect2.Protect2Byte);

    // 设置之后需要重启吗？先试试
    // MCU_RESET();
}
