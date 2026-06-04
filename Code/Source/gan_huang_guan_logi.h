#ifndef GAN_HUANG_GUAN_LOGI_H
#define GAN_HUANG_GUAN_LOGI_H

bool is_open_gan1(void);
bool is_open_gan2(void);
bool is_open_gan3(void);
bool is_open_gan4(void);
bool is_water_in(void);
bool is_charger_online(void);

void ganhuangguan_Logi(void);
UINT8 ganhuangguan_IsChargeLatched(void);
void ganhuangguan_WaitGan3ReleaseBeforeLongPress(void);

#endif
