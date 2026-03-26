#include "main.h"

bool is_open_gan1(void)
{
    return 0 == GPIO_ReadInputDataBit(GPIO_GAN1, PIN_GAN1);
}
bool is_open_gan2(void)
{
    return 0 == GPIO_ReadInputDataBit(GPIO_GAN2, PIN_GAN2);
}
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
    // return 0 == GPIO_ReadInputDataBit(GPIO_GAN2, PIN_GAN2);
    return true;
}

void ganhuangguan_Logi(void)
{
    if (is_water_in())
    {
        Driver_Element.DriverForceExt.bits.b2_DriverOFF_Flag = FORCE_CLOSE_MODE;
        // todo led±¨¾¯
    }
    else
    {
        if (is_open_gan1() && is_open_gan2() && is_open_gan3())
        {
            Driver_Element.DriverForceExt.bits.b2_DriverOFF_Flag = FORCE_KEEP_MODE;
        }
        else if (is_open_gan3())
        {
            // 2hÐÝÃß
        }
    }
}