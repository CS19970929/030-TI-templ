#ifndef IO_CONTROL_H
#define IO_CONTROL_H

#include "IODrivers_030.h"

typedef enum _IO_STATUS {
OPEN = 1, CLOSE = 0
}IO_STATUS;


typedef enum _FUNC_STATUS {
CONT = 0, RECOVER = 1
}FUNC_STATUS;


enum RELAY_CTRL_STATUS {
	RELAY_PRE_DET,	
	RELAY_PRE_OPEN,
	RELAY_PRE_CLOSE,
	RELAY_MAIN_OPEN,
	RELAY_MAIN_CLOSE,
	RELAY_ALL_CLOSE
};


enum MOS_CTRL_STATUS {
	MOS_PRE_DET,	
	MOS_PRE_OPEN,
	MOS_PRE_CLOSE,
	MOS_MAIN_OPEN,
	MOS_MAIN_CLOSE,
	MOS_ALL_CLOSE
};


union Switch_OnOFF_Function {
    UINT32 all;
    struct Switch_OnOFF_Ctrl {
		UINT8 b1Switch1			:1;
		UINT8 b1Switch2			:1;
		UINT8 b1Switch3			:1;
		UINT8 b1Switch4			:1;
		UINT8 b1Switch5			:1;
		UINT8 b1Switch6			:1;
		UINT8 b1Switch7			:1;
		UINT8 b1Switch8			:1;

		UINT8 b1Switch9			:1;
		UINT8 b1Switch10		:1;
		UINT8 b1Switch11		:1;
		UINT8 b1Switch12		:1;
		UINT8 b1Switch13		:1;
		UINT8 b1Switch14		:1;
		UINT8 b1Switch15		:1;
		UINT8 b1Switch16		:1;

		UINT8 b1Switch17		:1;
		UINT8 b1Switch18		:1;
		UINT8 b1Switch19		:1;
		UINT8 b1Switch20		:1;
		UINT8 b1Switch21		:1;
		UINT8 b1Switch22		:1;
		UINT8 b1Switch23		:1;
		UINT8 b1Switch24		:1;

		UINT8 b1Switch25		:1;
		UINT8 b1Switch26		:1;
		UINT8 b1Switch27		:1;
		UINT8 b1Switch28		:1;
		UINT8 b1Switch29		:1;
		UINT8 b1Switch30		:1;
		UINT8 b1Switch31		:1;
		UINT8 b1Switch32		:1;
    }bits;
};


#define PreRelayCloseT 			10			//打开主接触器后，预充继电器关闭时间
#define PreDsgMOSCloseT 		10			//打开主放电管后，预充放电管关闭时间


/*
以下驱动选一个
*/
#define _MOS_SAME_DOOR_NO_PRECHG
//#define _MOS_SAME_DOOR_HAVE_PRECHG
//#define _MOS_BOOTSTRAP_CIR

//#define _RELAY_SAME_DOOR_NO_PRECHG
//#define _RELAY_SAME_DOOR_HAVE_PRECHG
//#define _RELAY_DIFF_DOOR_NO_PRECHG
//#define _RELAY_DIFF_DOOR_HAVE_PRECHG


#if (defined _MOS_SAME_DOOR_NO_PRECHG)||(defined _MOS_SAME_DOOR_HAVE_PRECHG)||(defined _MOS_OTHER)||(defined _MOS_BOOTSTRAP_CIR)
#define _MOS
#else
#define _RELAY
#endif



//extern enum RELAY_CTRL_STATUS RelayCtrl_Command;
extern volatile union Switch_OnOFF_Function Switch_OnOFF_Func;
extern UINT8 gu8_DsgFirstOpen_Flag;


void InitMosRelay_DOx(void);
void InitData_Drivers(void);
void App_MOS_Relay_Ctrl(void);

#endif	/* IO_CONTROL_H */

