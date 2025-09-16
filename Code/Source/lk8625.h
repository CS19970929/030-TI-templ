#ifndef lk8625_H
#define lk8625_H

#define LK8625_ENTER_SLEEP()           GPIO_SetBits(GPIO_SLP_BLE, PIN_SLP_BLE)    
#define LK8625_EXIT_SLEEP()           GPIO_ResetBits(GPIO_SLP_BLE, PIN_SLP_BLE)   

void lk8625_init(void);
void lk8625_SendAT(char *_Cmd);

#endif
