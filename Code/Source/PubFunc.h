#ifndef PUBFUNC_H
#define PUBFUNC_H

typedef enum
{
    RESET_TYPE_NONE = 0,
    RESET_TYPE_LOW_POWER,
    RESET_TYPE_WINDOW_WDG,
    RESET_TYPE_INDEPENDENT_WDG,
    RESET_TYPE_SOFTWARE,
    RESET_TYPE_POR, // Power-On Reset
    RESET_TYPE_PIN, // NRST pin reset
    RESET_TYPE_UNKNOWN
} Reset_TypeDef;


typedef	struct{
	UINT16	u16ChkVal;                      // ��ǰ�Ƚ�����С
	UINT16	u16OPValB;                      // �Ƚϴ�ֵ
	UINT16	u16OPValS;                      // �Ƚ�Сֵ
    UINT16   *i16ChkCnt;                     // ָ���ʱ����ַ
    UINT16  u16TimeCntB;                    // ������ֵʱ��
	UINT16  u16TimeCntS;                    // ����Сֵʱ��
    UINT8	u8FlagLogic;                    // ���߼� 1��u8Flag=s_u16ChkVal > s_u16OPValB ? 1 : 0 ���߼� 1��u8Flag=s_u16ChkVal > s_u16OPValB ? 0 : 1
	UINT8	u8FlagBit;                      // �жϽ��
}SPUBOPUPCHK;

typedef enum {ODD = 0, EVEN = !ODD} Parity;


UINT16 Sci_CRC16RTU( UINT8 * pszBuf, UINT8 unLength);
UINT8 App_PubOPUPChk(SPUBOPUPCHK *t_sPubOPChk);
UINT16 GetEndValue(const UINT16 * ptbl,UINT16 tblsize,UINT16 dat);
unsigned char CRC8(unsigned char *ptr, unsigned char len,unsigned char key);
Parity OddEven_Check(UINT8 v);
UINT16 Usart_9bitOddEvenData_Frame(UINT8 data, Parity Parity_type);
UINT32 ModulusSub(UINT32 Data1, UINT32 Data2);
void Delay_Base10us(int n);
UINT8 Monitor_TempBreak(UINT16* temp_AD);

Reset_TypeDef MCU_GetResetType(void);
void toggleLed(void);

#endif	/* PUBFUNC_H */


