#include "main.h"

// 插头在位干簧管
bool is_open_gan1(void)
{
    return 0 == GPIO_ReadInputDataBit(GPIO_GAN1, PIN_GAN1);
    // return sys_time.test_1;
}
// 提手在位干簧管
bool is_open_gan2(void)
{
    return 0 == GPIO_ReadInputDataBit(GPIO_GAN2, PIN_GAN2);
    // return sys_time.test_2;
}
// 灯板开机控制
bool is_open_gan3(void)
{
    return 0 == GPIO_ReadInputDataBit(GPIO_GAN3, PIN_GAN3);
}
bool is_open_gan4(void)
{
    return 0 == GPIO_ReadInputDataBit(GPIO_GAN4, PIN_GAN4);
}
bool is_water_in(void)
{
    // todo
    return 0 == GPIO_ReadInputDataBit(GPIO_SWT_AD, PIN_SWT_AD);
}

void ganhuangguan_Logi(void)
{
    static uint16_t sleep_cnt = 0;
    if (!is_open_gan1() && (++sleep_cnt >= (100 * 2)))
    {
        entersleep(DEEP_MODE);
        sleep_reason = 2;
    }
    else if (is_water_in())
    {
        sleep_cnt = 0;
        Driver_Element.DriverForceExt.bits.b2_DriverOFF_Flag = FORCE_CLOSE_MODE;
        // todo led±¨¾¯
        GPIO_WriteBit(GPIO_SWT_EN, PIN_SWT_EN, 0);
    }
    else
    {
        if (is_open_gan1() && is_open_gan2())
        {
            sleep_cnt = 0;
            Driver_Element.DriverForceExt.bits.b2_DriverOFF_Flag = FORCE_KEEP_MODE;
        }
        else
        {
            Driver_Element.DriverForceExt.bits.b2_DriverOFF_Flag = FORCE_CLOSE_MODE;
        }
        // else if (is_open_gan3())
        // {
        //     // 2hÐÝÃß
        // }
    }
}