#include "main.h"
#include "gan_huang_guan_logi.h"

/* 每路独立帧缓冲；协议业务在主循环运行，中断仅负责收帧和搬运发送字节。 */
struct RS485MSG g_stCurrentMsgPtr_SCI1;
struct RS485MSG g_stCurrentMsgPtr_SCI2;

typedef struct {
    USART_TypeDef *usart;
    struct RS485MSG *msg;
    UINT8 supportsP12;
    volatile UINT8 rxFault;       /* 只需记录本帧是否出错，无需累计次数。 */
    volatile UINT8 rxTimeoutTick;
    volatile UINT8 txTimeoutTick; /* 仅TX_BUSY时由10ms定时中断递增。 */
} SciPort;

#ifdef _COMMOM_UPPER_SCI1
static SciPort sci1 = { USART1, &g_stCurrentMsgPtr_SCI1, 0, 0, 0, 0 };
#endif
#ifdef _COMMOM_UPPER_SCI2
static SciPort sci2 = { USART2, &g_stCurrentMsgPtr_SCI2, 1, 0, 0, 0 };
#endif

#define SCI_TX_TIMEOUT_TICKS 20u /* 最短约190ms，大于251字节/19200/8N1的131ms。 */
#define SCI_RX_TIMEOUT_TICKS 3u  /* 保留原有三个10ms调度节拍超时。 */

static volatile UINT32 s_rs485Tick10ms;
static volatile UINT32 s_rs485LastActivity;
static volatile UINT8 s_rs485PowerOn;
static volatile UINT8 s_rs485Ready;

UINT8 g_u8SCITxBuff[SCI_TX_BUF_LEN];

struct stCell_Info g_stCellInfoReport;
UINT8 u8FlashUpdateFlag = 0;
UINT8 u8FlashUpdateE2PROM = 0;

void Sci_WrRegs_0x10_CalibCoef(UINT16 u16Channel, struct RS485MSG *s);
void Sci_WrRegs_0x10_Protect(UINT16 u16Channel, struct RS485MSG *s);
void Sci_WrRegs_0x10_SocTable(struct RS485MSG *s);
void Sci_WrRegs_0x10_CopperLoss(struct RS485MSG *s);
void Sci_WrRegs_0x10_RTC(struct RS485MSG *s);
void Sci_WrRegs_0x10_Balance(struct RS485MSG *s);
void Sci_WrRegs_0x10_SysOther(struct RS485MSG *s);
void Sci_WrRegs_0x10_SleepElement(struct RS485MSG *s);
void Sci_WrRegs_0x10_SocElement(struct RS485MSG *s);
void Sci_WrRegs_0x10_SystemElement(struct RS485MSG *s);
void Sci_WrRegs_0x10_HeatCoolElement(struct RS485MSG *s);
void Sci_WrRegs_0x10_FlashConnect(struct RS485MSG *s);
void Sci_WrRegs_0x10_SN_Version(UINT16 startADDR, struct RS485MSG *s);

void Sci_WrReg_0x06_Reset_CalibCoef(struct RS485MSG *s);
void Sci_WrReg_0x06_Reset_ProtectRecord(struct RS485MSG *s);
void Sci_WrReg_0x06_Reset_ProtectElement(struct RS485MSG *s);
void Sci_WrReg_0x06_Reset_OtherCanAdd(struct RS485MSG *s);
void Sci_WrReg_0x06_Reset_HeatCool(struct RS485MSG *s);
void Sci_WrReg_0x06_SwitchON(struct RS485MSG *s);
void Sci_WrReg_0x06_SwitchOFF(struct RS485MSG *s);
void Sci_WrReg_0x06_BMS_FunctionON(struct RS485MSG *s);
void Sci_WrReg_0x06_BMS_FunctionOFF(struct RS485MSG *s);
void Sci_WrReg_0x06_SetSocOnce(struct RS485MSG *s);

static UINT16 P12_CalcChecksum(const UINT8 *buffer, UINT8 length);
static UINT8 P12_IsSupportedAddress(UINT16 address);
static UINT16 P12_GetVersionWord(void);
static UINT16 P12_GetRatedCapacityMah(void);
static UINT32 P12_GetStatusFlags(void);
static void P12_CopyAsciiField(UINT8 *dst, UINT8 dstLen, const UINT8 *src, UINT16 srcLen);
static UINT8 P12_VerifyFrame(struct RS485MSG *s);
static UINT8 P12_BuildResponse(struct RS485MSG *s);
static void Sci_ClearFrameState(struct RS485MSG *s);
static void Sci_ResetFrameState(SciPort *port);
static UINT8 Sci_IsValidWriteRegsByteCount(struct RS485MSG *s);
static void Sci_StartTx(SciPort *port);
static void Sci_TxISR_Deal(SciPort *port);

void Sci_DataInit(struct RS485MSG *s)
{
	UINT16 i;

	s->ptr_no = 0;
	s->csr = RS485_STA_IDLE;
	s->enRs485CmdType = RS485_CMD_READ_REGS;
	s->AckLenth = 0;
	s->AckType = RS485_ACK_POS;
	s->ErrorType = RS485_ERROR_NULL;
	s->u8FrameProtocol = SCI_FRAME_PROTOCOL_MODBUS;
	s->u8FrameDataLength = 0;
	s->u8FrameCommand = 0;
	s->u16FrameAddress = 0;
	for (i = 0; i < RS485_MAX_BUFFER_SIZE; i++)
	{
		s->u16Buffer[i] = 0;
	}
	for (i = 0; i < SCI_TX_BUF_LEN; i++)
	{
		g_u8SCITxBuff[i] = 0;
	}
}

static UINT16 P12_CalcChecksum(const UINT8 *buffer, UINT8 length)
{
	UINT16 sum = 0;
	UINT16 i;

	for (i = 0; i < length; ++i)
	{
		sum = (UINT16)(sum + buffer[i]);
	}

	return sum;
}

static UINT8 P12_IsSupportedAddress(UINT16 address)
{
	return (UINT8)((address == P12_BMS_ADDR_DEFAULT) || (address == P12_BMS_ADDR_EXAMPLE_ALIAS));
}

static UINT16 P12_GetVersionWord(void)
{
	UINT16 mainVer = 1;
	UINT16 minorVer = 0;
	UINT16 buildVer = 1;
	UINT16 value = 0;
	UINT16 index;
	UINT16 length = ProductionInfor.BMS_SoftWareVersionLength;
	UINT16 start = 0;
	UINT16 part = 0;
	UINT8 dotCount = 0;
	UINT8 hasDigit = 0;

#if 0
	if ((0 == length) || (length > PRODUCT_ID_LENGTH_MAX))
	{
		length = PRODUCT_ID_LENGTH_MAX;
	}

	for (index = 0; index < length; ++index)
	{
		if (ProductionInfor.BMS_SoftWareVersion[index] == '.')
		{
			dotCount++;
		}
		if (0 == ProductionInfor.BMS_SoftWareVersion[index])
		{
			break;
		}
	}

	if (dotCount < 2)
	{
		return 1001;
	}

	for (index = 0; index <= length; ++index)
	{
		UINT8 ch = (index < length) ? ProductionInfor.BMS_SoftWareVersion[index] : 0;
		if ((ch >= '0') && (ch <= '9'))
		{
			value = (UINT16)(value * 10 + (ch - '0'));
			hasDigit = 1;
		}
		else if ((ch == '.') || (0 == ch))
		{
			if (!hasDigit)
			{
				return 1001;
			}
			if (0 == part)
			{
				mainVer = value;
			}
			else if (1 == part)
			{
				minorVer = value;
			}
			else
			{
				buildVer = value;
				break;
			}
			part++;
			value = 0;
			hasDigit = 0;
			start = index + 1;
		}
		else if (index > start)
		{
			break;
		}
	}

	mainVer %= 10;
	minorVer %= 10;
	buildVer %= 100;

#endif
	return (UINT16)(mainVer * 1000 + minorVer * 100 + buildVer);
}

static UINT16 P12_GetRatedCapacityMah(void)
{
	UINT32 capacityMah;

	if (g_stCellInfoReport.SocElement.u16CapacityFactory)
	{
		capacityMah = (UINT32)g_stCellInfoReport.SocElement.u16CapacityFactory * 10u;
	}
	else
	{
		capacityMah = (UINT32)OtherElement.u16Soc_Ah * 10u;
	}

	if (capacityMah > 0xFFFFu)
	{
		capacityMah = 0xFFFFu;
	}

	return (UINT16)capacityMah;
}

static UINT32 P12_GetStatusFlags(void)
{
	UINT32 flags = 0;
	struct MDLCHGFAULT_BITS faultBits = g_stCellInfoReport.unMdlFault_Third.bits;

	if (faultBits.b1SocLow)
	{
		flags |= (1ul << 0);
	}
	if (faultBits.b1IdischgOcp)
	{
		flags |= (1ul << 1);
	}
	if (faultBits.b1CellDischgOtp)
	{
		flags |= (1ul << 2);
	}
	if (faultBits.b1IchgOcp)
	{
		flags |= (1ul << 3);
	}
	if (faultBits.b1CellChgOtp)
	{
		flags |= (1ul << 4);
	}
	if (faultBits.b1CellDischgUtp)
	{
		flags |= (1ul << 5);
	}
	if (faultBits.b1CellChgUtp)
	{
		flags |= (1ul << 6);
	}
	if (faultBits.b1BatOvp)
	{
		flags |= (1ul << 7);
	}
	if (faultBits.b1BatUvp)
	{
		flags |= (1ul << 8);
	}
	if (faultBits.b1CellOvp)
	{
		flags |= (1ul << 9);
	}
	if (faultBits.b1CellUvp)
	{
		flags |= (1ul << 10);
	}
	if (faultBits.b1VcellDeltaBig)
	{
		flags |= (1ul << 11);
	}
	if (faultBits.b1TmosOtp)
	{
		flags |= (1ul << 12);
	}
	if (is_water_in())
	{
		flags |= (1ul << 13);
	}
	if (is_open_gan1())
		flags |= (1ul << 14);
	if (is_open_gan2())
		flags |= (1ul << 15);

	return flags;
}

static void P12_CopyAsciiField(UINT8 *dst, UINT8 dstLen, const UINT8 *src, UINT16 srcLen)
{
	UINT8 i;

	if (srcLen > dstLen)
	{
		srcLen = dstLen;
	}

	for (i = 0; i < dstLen; ++i)
	{
		dst[i] = 0;
	}

	for (i = 0; i < srcLen; ++i)
	{
		if (0 == src[i])
		{
			break;
		}
		dst[i] = src[i];
	}
}

static UINT8 P12_VerifyFrame(struct RS485MSG *s)
{
	UINT16 checksum;
	UINT16 expectedChecksum;

	s->u16FrameAddress = (UINT16)(s->u16Buffer[2] | (s->u16Buffer[3] << 8));
	s->u8FrameCommand = s->u16Buffer[4];
	s->u8FrameDataLength = s->u16Buffer[5];

	if (((UINT16)s->u8FrameDataLength + 8u) != s->ptr_no)
	{
		return 0;
	}

	if (!P12_IsSupportedAddress(s->u16FrameAddress))
	{
		return 0;
	}

	checksum = (UINT16)(s->u16Buffer[6 + s->u8FrameDataLength] | (s->u16Buffer[7 + s->u8FrameDataLength] << 8));
	expectedChecksum = P12_CalcChecksum((UINT8 *)s->u16Buffer, (UINT8)(6 + s->u8FrameDataLength));
	return (UINT8)(checksum == expectedChecksum);
}

static UINT8 P12_BuildResponse(struct RS485MSG *s)
{
	UINT8 dataIndex = 6;
	UINT8 i;
	UINT8 seriesNum;
	UINT16 value16;
	UINT16 checksum;
	UINT16 cycleTimes;
	UINT32 value32;
	INT32 currentMa;

	s->u16Buffer[0] = P12_FRAME_HEAD0;
	s->u16Buffer[1] = P12_FRAME_HEAD1;
	s->u16Buffer[2] = (UINT8)(P12_HOST_ADDR & 0x00FF);
	s->u16Buffer[3] = (UINT8)(P12_HOST_ADDR >> 8);

	switch (s->u8FrameCommand)
	{
	case P12_CMD_REQ_STATIC:
		if (0 != s->u8FrameDataLength)
		{
			return 0;
		}
		s->u16Buffer[4] = P12_CMD_RSP_STATIC;
		s->u16Buffer[5] = 19;
		value16 = P12_GetRatedCapacityMah();
		s->u16Buffer[dataIndex++] = (UINT8)(value16 & 0x00FF);
		s->u16Buffer[dataIndex++] = (UINT8)(value16 >> 8);
		value16 = P12_GetVersionWord();
		s->u16Buffer[dataIndex++] = (UINT8)(value16 & 0x00FF);
		s->u16Buffer[dataIndex++] = (UINT8)(value16 >> 8);
		P12_CopyAsciiField(&s->u16Buffer[dataIndex], 14, ProductionInfor.BMS_SerialNumber, ProductionInfor.BMS_SerialNumberLength);
		dataIndex += 14;
		s->u16Buffer[dataIndex++] = 0x00;
		break;

	case P12_CMD_REQ_DYNAMIC:
		if (0 != s->u8FrameDataLength)
		{
			return 0;
		}
		seriesNum = SeriesNum;
		if ((0 == seriesNum) || (seriesNum > 32))
		{
			seriesNum = (UINT8)OtherElement.u16Sys_SeriesNum;
		}
		if ((0 == seriesNum) || (seriesNum > 32))
		{
			seriesNum = 8;
		}
		s->u16Buffer[4] = P12_CMD_RSP_DYNAMIC;
		s->u16Buffer[5] = (UINT8)(16 + seriesNum * 2);

		value32 = (UINT32)g_stCellInfoReport.u16VCellTotle * 10u;
		if (value32 > 0xFFFFu)
		{
			value32 = 0xFFFFu;
		}
		value16 = (UINT16)value32;
		s->u16Buffer[dataIndex++] = (UINT8)(value16 & 0x00FF);
		s->u16Buffer[dataIndex++] = (UINT8)(value16 >> 8);

		if (g_stCellInfoReport.u16Ichg)
		{
			currentMa = (INT32)g_stCellInfoReport.u16Ichg * 100;
		}
		else
		{
			currentMa = -((INT32)g_stCellInfoReport.u16IDischg * 100);
		}
		value32 = (UINT32)currentMa;
		s->u16Buffer[dataIndex++] = (UINT8)(value32 & 0x000000FFul);
		s->u16Buffer[dataIndex++] = (UINT8)((value32 >> 8) & 0x000000FFul);
		s->u16Buffer[dataIndex++] = (UINT8)((value32 >> 16) & 0x000000FFul);
		s->u16Buffer[dataIndex++] = (UINT8)((value32 >> 24) & 0x000000FFul);

		value16 = g_stCellInfoReport.u16TempMax;
		if (0 == value16)
		{
			value16 = g_stCellInfoReport.u16Temperature[0];
		}
		s->u16Buffer[dataIndex++] = (UINT8)(value16 & 0x00FF);
		s->u16Buffer[dataIndex++] = (UINT8)(value16 >> 8);

		cycleTimes = g_stCellInfoReport.SocElement.u16Cycle_times;
		if (0 == cycleTimes)
		{
			cycleTimes = OtherElement.u16Soc_Cycle_times;
		}
		s->u16Buffer[dataIndex++] = (UINT8)(cycleTimes & 0x00FF);
		s->u16Buffer[dataIndex++] = (UINT8)(cycleTimes >> 8);

		value16 = g_stCellInfoReport.SocElement.u16Soc;
		if (value16 > 100)
		{
			value16 = 100;
		}
		s->u16Buffer[dataIndex++] = (UINT8)value16;
		s->u16Buffer[dataIndex++] = seriesNum;

		for (i = 0; i < seriesNum; ++i)
		{
			value16 = g_stCellInfoReport.u16VCell[i];
			s->u16Buffer[dataIndex++] = (UINT8)(value16 & 0x00FF);
			s->u16Buffer[dataIndex++] = (UINT8)(value16 >> 8);
		}

		value32 = P12_GetStatusFlags();
		s->u16Buffer[dataIndex++] = (UINT8)(value32 & 0x000000FFul);
		s->u16Buffer[dataIndex++] = (UINT8)((value32 >> 8) & 0x000000FFul);
		s->u16Buffer[dataIndex++] = (UINT8)((value32 >> 16) & 0x000000FFul);
		s->u16Buffer[dataIndex++] = (UINT8)((value32 >> 24) & 0x000000FFul);
		break;

	default:
		return 0;
	}

	checksum = P12_CalcChecksum((UINT8 *)s->u16Buffer, dataIndex);
	s->u16Buffer[dataIndex++] = (UINT8)(checksum & 0x00FF);
	s->u16Buffer[dataIndex++] = (UINT8)(checksum >> 8);
	s->AckLenth = dataIndex;
	s->AckType = RS485_ACK_POS;
	s->ptr_no = 0;
	return 1;
}

static void Sci_ClearFrameState(struct RS485MSG *s)
{
	s->ptr_no = 0;
	s->csr = RS485_STA_IDLE;
	s->enRs485CmdType = RS485_CMD_READ_REGS;
	s->u16RdRegStartAddr = 0;
	s->u16RdRegStartAddrActure = 0;
	s->u16RdRegByteNum = 0;
	s->AckLenth = 0;
	s->AckType = RS485_ACK_POS;
	s->ErrorType = RS485_ERROR_NULL;
	s->u8FrameProtocol = SCI_FRAME_PROTOCOL_MODBUS;
	s->u8FrameDataLength = 0;
	s->u8FrameCommand = 0;
	s->u16FrameAddress = 0;
	s->u16Buffer[0] = 0;
	s->u16Buffer[1] = 0;
	s->u16Buffer[2] = 0;
	s->u16Buffer[3] = 0;
	s->u16Buffer[4] = 0;
	s->u16Buffer[5] = 0;
	s->u16Buffer[6] = 0;
}

/* 仅在未发送或TC完成后调用；清理帧与重新开放接收必须先后完成。 */
static void Sci_ResetFrameState(SciPort *port)
{
    USART_TypeDef *usart = port->usart;
    usart->CR1 &= ~(USART_CR1_RXNEIE | USART_CR1_TXEIE | USART_CR1_TCIE);
    Sci_ClearFrameState(port->msg);
    port->rxTimeoutTick = 0;
    port->txTimeoutTick = 0;
    port->rxFault = 0;
    USART_ClearITPendingBit(usart, USART_IT_TC);
    usart->CR1 |= USART_CR1_RE | USART_CR1_RXNEIE;
}

/* PB1原M_STB现为串口2的485方向：高发送，低接收。 */
static void Sci_SetTxDirection(SciPort *port, UINT8 transmitting)
{
    if (port->usart == USART2)
    {
        if (transmitting)
            GPIO_SetBits(GPIO_M_STB, PIN_M_STB);
        else
            GPIO_ResetBits(GPIO_M_STB, PIN_M_STB);
    }
}

/* PB14 activates RS485 only while the MCU is running. */
void Sci_RS485WakeInit(void)
{
    GPIO_InitTypeDef gpio;
    EXTI_InitTypeDef exti;
    NVIC_InitTypeDef nvic;
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOB, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = PIN_INT_WK_CMNT;
    gpio.GPIO_Mode = GPIO_Mode_IN;
    gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIO_INT_WK_CMNT, &gpio);
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOB, EXTI_PinSource14);
    exti.EXTI_Line = EXTI_Line14;
    exti.EXTI_Mode = EXTI_Mode_Interrupt;
    exti.EXTI_Trigger = EXTI_Trigger_Rising;
    exti.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti);
    nvic.NVIC_IRQChannel = EXTI4_15_IRQn;
    nvic.NVIC_IRQChannelPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
}

UINT8 Sci_RS485PowerIsOn(void)
{
    return s_rs485PowerOn;
}

void Sci_RS485WakeFromISR(void)
{
    if (!s_rs485Ready)
        return;

    /* Repeated wake edges while powered do not extend the window. */
    if (!s_rs485PowerOn)
    {
#ifdef _COMMOM_UPPER_SCI2
        if (s_rs485Ready)
        {
            USART_ReceiveData(USART2);
            USART_ClearFlag(USART2, USART_FLAG_ORE | USART_FLAG_NE |
                                    USART_FLAG_FE | USART_FLAG_PE);
            Sci_ResetFrameState(&sci2);
        }
#endif
        GPIO_SetBits(GPIO_M_CTR, PIN_M_CTR);
        s_rs485LastActivity = s_rs485Tick10ms;
        s_rs485PowerOn = 1;
    }
}

static void Sci_RS485ValidRequest(SciPort *port)
{
    UINT32 primask = __get_PRIMASK();
    __disable_irq();
    if ((port->usart == USART2) && s_rs485PowerOn)
        s_rs485LastActivity = s_rs485Tick10ms;
    __set_PRIMASK(primask);
}

static void Sci_RS485PowerService(void)
{
#ifdef _COMMOM_UPPER_SCI2
    UINT32 primask = __get_PRIMASK();
    __disable_irq();
    /* Complete validated requests and TX before cutting power. Partial/noisy
     * frames cannot keep the supply on; TX watchdog bounds a stuck transfer. */
    if (s_rs485PowerOn &&
        ((UINT32)(s_rs485Tick10ms - s_rs485LastActivity) >=
         (UINT32)RS485_POWER_WINDOW_SECONDS * 100u) &&
        (sci2.msg->csr == RS485_STA_IDLE))
    {
        Sci_ResetFrameState(&sci2);
        Sci_SetTxDirection(&sci2, 0);
        USART2->CR1 &= ~(USART_CR1_RE | USART_CR1_RXNEIE);
        GPIO_ResetBits(GPIO_M_CTR, PIN_M_CTR);
        s_rs485PowerOn = 0;
    }
    __set_PRIMASK(primask);
#endif
}

static UINT8 Sci_IsValidWriteRegsByteCount(struct RS485MSG *s)
{
	UINT16 regNum;
	UINT16 byteCount;

	regNum = (UINT16)(s->u16Buffer[5] + (s->u16Buffer[4] << 8));
	byteCount = s->u16Buffer[6];

	if (0 == regNum)
	{
		return 0;
	}
	if (((UINT32)regNum * 2u) != byteCount)
	{
		return 0;
	}
	if (((UINT16)byteCount + 9u) > RS485_MAX_BUFFER_SIZE)
	{
		return 0;
	}

	return 1;
}

static void Sci_StartTx(SciPort *port)
{
    struct RS485MSG *s = port->msg;
    USART_TypeDef *usart = port->usart;
    if ((0 == s->AckLenth) || (s->AckLenth > RS485_MAX_BUFFER_SIZE))
    {
        Sci_ResetFrameState(port);
        return;
    }
    usart->CR1 &= ~(USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_TXEIE | USART_CR1_TCIE);
    port->txTimeoutTick = 0;
    s->ptr_no = 0;
    s->csr = RS485_STA_TX_BUSY;
    Sci_SetTxDirection(port, 1);
    usart->CR1 |= USART_CR1_TE;
    usart->ICR = USART_ICR_TCCF;
    /* 首字节也由TXE搬运，避免两份发送/末字节判断逻辑。 */
    usart->CR1 |= USART_CR1_TXEIE;
}

/* TC确认后立即准备接收，再释放485方向；主循环不再二次清帧。 */
static void Sci_FinishTx(SciPort *port)
{
    if (u8FlashUpdateE2PROM)
    {
        u8FlashUpdateE2PROM = 0;
        u8FlashUpdateFlag = 1;
    }
    Sci_ResetFrameState(port);
    Sci_SetTxDirection(port, 0);
}

/* 超时是故障中止，不是正常完成；不能仅清TE后立即释放方向。
 * RM0360: UE=0立即停止输出并丢弃当前传输，配置保留。
 */
static void Sci_AbortTx(SciPort *port)
{
    USART_TypeDef *usart = port->usart;
    usart->CR1 &= ~(USART_CR1_TXEIE | USART_CR1_TCIE | USART_CR1_RXNEIE);
    USART_Cmd(usart, DISABLE);
    usart->CR1 &= ~USART_CR1_TE;
    Sci_ResetFrameState(port);
    /* UE仍关闭，先释放总线再使能接收；下次发送由StartTx重新开启TE。 */
    Sci_SetTxDirection(port, 0);
    USART_Cmd(usart, ENABLE);
}

static void Sci_TxWatchdog(SciPort *port)
{
    if ((RS485_STA_TX_BUSY == port->msg->csr) &&
        (++port->txTimeoutTick >= SCI_TX_TIMEOUT_TICKS))
    {
        /* 末字节已装载且TC确实置位：补做丢失的完成中断，不误判失败。 */
        if ((port->msg->ptr_no == port->msg->AckLenth) &&
            (port->msg->AckLenth != 0) &&
            ((port->usart->ISR & USART_ISR_TC) != RESET))
            Sci_FinishTx(port);
        else
            Sci_AbortTx(port);
    }
}

/* TIM17与USART当前均为优先级0，互不抢占；这里无等待、无协议业务。 */
void Sci_Tick10ms(void)
{
    ++s_rs485Tick10ms;
#ifdef _COMMOM_UPPER_SCI1
    Sci_TxWatchdog(&sci1);
#endif
#ifdef _COMMOM_UPPER_SCI2
    Sci_TxWatchdog(&sci2);
#endif
}

static void Sci_TxISR_Deal(SciPort *port)
{
    struct RS485MSG *s = port->msg;
    USART_TypeDef *usart = port->usart;
    if (RS485_STA_TX_BUSY != s->csr)
    {
        usart->CR1 &= ~(USART_CR1_TXEIE | USART_CR1_TCIE);
        return;
    }
    if (((usart->CR1 & USART_CR1_TXEIE) != RESET) &&
        ((usart->ISR & USART_ISR_TXE) != RESET))
    {
        if (s->ptr_no < s->AckLenth)
        {
            usart->TDR = s->u16Buffer[s->ptr_no++];
        }
        if (s->ptr_no >= s->AckLenth)
        {
            usart->CR1 &= ~USART_CR1_TXEIE;
            /* 写TDR后等待TC；TXE只代表数据寄存器空，不代表停止位发完。 */
            usart->CR1 |= USART_CR1_TCIE;
        }
        /* 不在写入末字节的这次处理里消费TC，避免使用过期状态。 */
        return;
    }
    if (((usart->CR1 & USART_CR1_TCIE) != RESET) &&
        ((usart->ISR & USART_ISR_TC) != RESET))
    {
        Sci_FinishTx(port);
    }
}

void CRC_verify(struct RS485MSG *s)
{
	UINT16 u16SciVerify;
	UINT16 t_u16FrameLenth;

	t_u16FrameLenth = s->ptr_no - 2;
	u16SciVerify = s->u16Buffer[t_u16FrameLenth] + (s->u16Buffer[t_u16FrameLenth + 1] << 8);
	if (u16SciVerify == Sci_CRC16RTU((UINT8 *)s->u16Buffer, t_u16FrameLenth))
	{
		s->AckType = RS485_ACK_POS;
	}
	else
	{
		s->u16RdRegByteNum = 0;
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_CRC_ERROR;
	}
}

void Sci_Deal_ReadRegs_0x03(struct RS485MSG *s)
{
	UINT16 t_u16Temp;

	t_u16Temp = s->u16Buffer[3] + (s->u16Buffer[2] << 8);
	s->u16RdRegStartAddrActure = t_u16Temp;

	if (t_u16Temp >= RS485_ADDR_RO_START2)
	{ // 1个字
		t_u16Temp -= (RS485_ADDR_RO_START2 - 63 - 33);
	}

	else if (t_u16Temp >= RS485_ADDR_RO_START1)
	{ // 33个字
		t_u16Temp -= (RS485_ADDR_RO_START1 - 63);
	}

	else if (t_u16Temp >= RS485_ADDR_RO_START0)
	{ // 63个字
		t_u16Temp -= RS485_ADDR_RO_START0;
	}
	// 新加进来的
	else if (t_u16Temp >= RS485_ADDR_RO_LCD)
	{
		t_u16Temp -= RS485_ADDR_RO_LCD; // LCD，有一次顺序乱了，显示数据不对导致找不到原因
	}
	else if (t_u16Temp >= RS485_ADDR_RW_OTHER_CANADD)
	{
		t_u16Temp -= RS485_ADDR_RW_OTHER_CANADD;
	}
	else if (t_u16Temp >= RS485_ADDR_RW_OTHER)
	{
		t_u16Temp -= RS485_ADDR_RW_OTHER;
	}
	else if (t_u16Temp >= RS485_ADDR_RW_PORTECT)
	{
		t_u16Temp -= RS485_ADDR_RW_PORTECT;
	}
	else if (t_u16Temp >= RS485_ADDR_RW_CALIB)
	{
		t_u16Temp -= RS485_ADDR_RW_CALIB;
	}

	s->u16RdRegStartAddr = t_u16Temp;
	s->u16RdRegByteNum = (s->u16Buffer[5] + (s->u16Buffer[4] << 8)) << 1;
}

void Sci_Deal_WrReg_0x06(struct RS485MSG *s)
{
	UINT16 u16SciRegAddr;
	u16SciRegAddr = s->u16Buffer[3] + (s->u16Buffer[2] << 8);
	switch (u16SciRegAddr)
	{
	case RS485_CMD_ADDR_RESET_CALIB_COEF:
		Sci_WrReg_0x06_Reset_CalibCoef(s);
		break;

	case RS485_CMD_ADDR_RESET_PROTECT_RECORD:
		Sci_WrReg_0x06_Reset_ProtectRecord(s);
		break;

	case RS485_CMD_ADDR_RESET_PROTECT_ELEMENT:
		Sci_WrReg_0x06_Reset_ProtectElement(s);
		break;

	case RS485_CMD_ADDR_RESET_OTHER_CANADD:
		Sci_WrReg_0x06_Reset_OtherCanAdd(s);
		break;

	case RS485_CMD_ADDR_RESET_HEAT_COOL:
		Sci_WrReg_0x06_Reset_HeatCool(s);
		break;

	case RS485_CMD_ADDR_SWITCH_ON:
		Sci_WrReg_0x06_SwitchON(s);
		break;

	case RS485_CMD_ADDR_SWITCH_OFF:
		Sci_WrReg_0x06_SwitchOFF(s);
		break;

	case RS485_CMD_ADDR_SYSTEM_FUNCTION_ON:
		Sci_WrReg_0x06_BMS_FunctionON(s);
		break;

	case RS485_CMD_ADDR_SYSTEM_FUNCTION_OFF:
		Sci_WrReg_0x06_BMS_FunctionOFF(s);
		break;

	case RS485_CMD_ADDR_SET_ONCE_SOC:
		Sci_WrReg_0x06_SetSocOnce(s);
		break;

	case RS485_CMD_ADDR_RESET_EVENT_RECORD:
		Sci_WrReg_0x06_Reset_EventRecord(s);
		break;

	default:
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_NO_PERMISSION;
		break;
	}
}

// 主体OK
void Sci_Deal_WrRegs_0x10(struct RS485MSG *s)
{
	UINT16 u16SciRegStartAddr;
	u16SciRegStartAddr = s->u16Buffer[3] + (s->u16Buffer[2] << 8);
	switch (u16SciRegStartAddr)
	{
	case RS485_CMD_ADDR_VC1CALIB_K:
	case RS485_CMD_ADDR_VC2CALIB_K:
	case RS485_CMD_ADDR_VC3CALIB_K:
	case RS485_CMD_ADDR_VC4CALIB_K:
	case RS485_CMD_ADDR_VC5CALIB_K:
	case RS485_CMD_ADDR_VC6CALIB_K:
	case RS485_CMD_ADDR_VC7CALIB_K:
	case RS485_CMD_ADDR_VC8CALIB_K:
	case RS485_CMD_ADDR_VC9CALIB_K:
	case RS485_CMD_ADDR_VC10CALIB_K:
	case RS485_CMD_ADDR_VC11CALIB_K:
	case RS485_CMD_ADDR_VC12CALIB_K:
	case RS485_CMD_ADDR_VC13CALIB_K:
	case RS485_CMD_ADDR_VC14CALIB_K:
	case RS485_CMD_ADDR_VC15CALIB_K:
	case RS485_CMD_ADDR_VC16CALIB_K:
	case RS485_CMD_ADDR_VC17CALIB_K:
	case RS485_CMD_ADDR_VC18CALIB_K:
	case RS485_CMD_ADDR_VC19CALIB_K:
	case RS485_CMD_ADDR_VC20CALIB_K:
	case RS485_CMD_ADDR_VC21CALIB_K:
	case RS485_CMD_ADDR_VC22CALIB_K:
	case RS485_CMD_ADDR_VC23CALIB_K:
	case RS485_CMD_ADDR_VC24CALIB_K:
	case RS485_CMD_ADDR_VC25CALIB_K:
	case RS485_CMD_ADDR_VC26CALIB_K:
	case RS485_CMD_ADDR_VC27CALIB_K:
	case RS485_CMD_ADDR_VC28CALIB_K:
	case RS485_CMD_ADDR_VC29CALIB_K:
	case RS485_CMD_ADDR_VC30CALIB_K:
	case RS485_CMD_ADDR_VC31CALIB_K:
	case RS485_CMD_ADDR_VC32CALIB_K:
	case RS485_CMD_ADDR_AFE1CALIB_K:
	case RS485_CMD_ADDR_AFE2CALIB_K:
	case RS485_CMD_ADDR_VBUSCALIB_K:
	case RS485_CMD_ADDR_ICHGCALIB_K:
	case RS485_CMD_ADDR_IDISCHGCALIB_K:
	case RS485_CMD_ADDR_TEMP1_CALIB_K:
	case RS485_CMD_ADDR_TEMP2_CALIB_K:
	case RS485_CMD_ADDR_TEMP3_CALIB_K:
	case RS485_CMD_ADDR_TEMP4_CALIB_K:
	case RS485_CMD_ADDR_TEMP5_CALIB_K:
	case RS485_CMD_ADDR_TEMP6_CALIB_K:
	case RS485_CMD_ADDR_TEMP_ENV1_CALIB_K:
	case RS485_CMD_ADDR_TEMP_ENV2_CALIB_K:
	case RS485_CMD_ADDR_TEMP_ENV3_CALIB_K:
	case RS485_CMD_ADDR_TEMP_MOS_CALIB_K:
		Sci_WrRegs_0x10_CalibCoef(u16SciRegStartAddr, s);
		break;

	case RS485_CMD_ADDR_VCELL_OVP_FIRST:
	case RS485_CMD_ADDR_VCELL_UVP_FIRST:
	case RS485_CMD_ADDR_VBUS_OVP_FIRST:
	case RS485_CMD_ADDR_VBUS_UVP_FIRST:
	case RS485_CMD_ADDR_ICHG_OCP_FIRST:
	case RS485_CMD_ADDR_IDSG_OCP_FIRST:
	case RS485_CMD_ADDR_TCHG_OTP_FIRST:
	case RS485_CMD_ADDR_TCHG_UTP_FIRST:
	case RS485_CMD_ADDR_TDSG_OTP_FIRST:
	case RS485_CMD_ADDR_TDSG_UTP_FIRST:
	case RS485_CMD_ADDR_TMOS_OTP_FIRST:
	case RS485_CMD_ADDR_VDELTA_OP_FIRST:
	case RS485_CMD_ADDR_SOC_UP_FIRST:
		Sci_WrRegs_0x10_Protect(u16SciRegStartAddr, s);
		break;

	case RS485_CMD_ADDR_SOC_VOLTAGE1:
		Sci_WrRegs_0x10_SocTable(s);
		break;

	case RS485_CMD_ADDR_COPPERLOSS1:
		Sci_WrRegs_0x10_CopperLoss(s);
		break;

	case RS485_CMD_ADDR_RTC_TIME_YEAR:
		Sci_WrRegs_0x10_RTC(s);
		break;

	case RS485_CMD_ADDR_BALANCE_OV:
		Sci_WrRegs_0x10_Balance(s);
		break;

	case RS485_CMD_ADDR_CS_CUR_CHGMAX:
		Sci_WrRegs_0x10_SysOther(s);
		break;

	case RS485_CMD_ADDR_SLEEP_V_NORMAL:
		Sci_WrRegs_0x10_SleepElement(s);
		break;

	case RS485_CMD_ADDR_SOC_AH:
		Sci_WrRegs_0x10_SocElement(s);
		break;

	case RS485_CMD_ADDR_SYS_SERIES_NUM:
		Sci_WrRegs_0x10_SystemElement(s);
		break;

	case RS485_CMD_ADDR_HEAT_DSG_HIGH:
		Sci_WrRegs_0x10_HeatCoolElement(s);
		break;

	case RS485_ADDR_SN_SERIAL_NUM:
	case RS485_ADDR_SN_HAEDWARE_VER:
	case RS485_ADDR_SN_SOFTWARE_VER:
		Sci_WrRegs_0x10_SN_Version(u16SciRegStartAddr, s);
		break;

	case RS485_CMD_ADDR_FLASH_CONNECT:
		Sci_WrRegs_0x10_FlashConnect(s);
		break; // 少了个BREAK导致OVER。
	default:
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_CMD_INVALID;
		break;
	}
}

void Sci_ACK_0x03_ReadRegs_LCD(struct RS485MSG *s, UINT8 t_u8BuffTemp[])
{
	UINT16 u16SciTemp;
	UINT16 i, j;
	INT8 k, x;

	i = 0;
	switch (s->u16RdRegStartAddr)
	{
	case 0: // LCD
		u16SciTemp = 1;
		t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
		t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

		u16SciTemp = (g_stCellInfoReport.u16VCellTotle + 50) / 100;
		t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
		t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

		if (g_stCellInfoReport.u16Ichg > 0)
		{
			u16SciTemp = (g_stCellInfoReport.u16Ichg + 5005) / 10;
		}
		else
		{
			u16SciTemp = (5000 - g_stCellInfoReport.u16IDischg) / 10;
		}
		t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
		t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

		u16SciTemp = (g_stCellInfoReport.u16TempMax + 5) / 10;
		t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
		t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

		u16SciTemp = g_stCellInfoReport.SocElement.u16Soc;
		t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
		t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
		break;

	case 1: // 上位机第三级保护，60+10=70个
		for (j = 0; j < Record_len; j++)
		{
			k = FaultPoint_Third - 1 - j;
			if (k < 0)
			{
				k = Record_len + k;
			}
			u16SciTemp = Fault_record_Third[k];
			t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
			t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

			for (x = 0; x < 6; ++x)
			{
				u16SciTemp = RTC_Fault_record_Third[k][x];
				t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
				t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
			}
		}
		break;

	case 2: // 序列号，硬件版本号，软件版本号
		for (j = 0; j < PRODUCT_ID_LENGTH_MAX; j++)
		{
			t_u8BuffTemp[i++] = ProductionInfor.BMS_SerialNumber[j];
		}
		for (j = 0; j < PRODUCT_ID_LENGTH_MAX; j++)
		{
			t_u8BuffTemp[i++] = ProductionInfor.BMS_HardWareVersion[j];
		}
		for (j = 0; j < PRODUCT_ID_LENGTH_MAX; j++)
		{
			t_u8BuffTemp[i++] = ProductionInfor.BMS_SoftWareVersion[j];
		}
		break;

	case 8:
		Sci_ACK_0x03_ReadRegs_EventRecord(t_u8BuffTemp);
		break;

	default:
		s->u16RdRegStartAddr = 0;
		break;
	}
	s->u16RdRegStartAddr = 0;
}

void Sci_ACK_0x03_ReadRegs_Data(struct RS485MSG *s, UINT8 t_u8BuffTemp[])
{
	UINT16 u16SciTemp;
	UINT16 i = 0, j;
	INT8 k;
	UINT8 a[4];

	for (j = 0; j < 63; j++)
	{ // 0xD000_63
		u16SciTemp = *(&g_stCellInfoReport.u16VCell[0] + j);
		t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
		t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
	}

	// 0xD100_33
	// u16SciTemp = (UINT16)(RTC_time.RTC_Time_Month) | (RTC_time.RTC_Time_Year<<8);
	u16SciTemp = 0;
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	// u16SciTemp = (UINT16)(RTC_time.RTC_Time_Hour) | (RTC_time.RTC_Time_Day<<8);
	u16SciTemp = 0;
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	// u16SciTemp = (UINT16)(RTC_time.RTC_Time_Second) | (RTC_time.RTC_Time_Minute<<8);
	u16SciTemp = 0;
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	for (j = 0; j < 4; j++)
	{
		k = FaultPoint_First2 - 1 - j;
		if (k < 0)
		{
			k = Record_len + k;
		}
		a[j] = k;
	}
	u16SciTemp = (Fault_record_First2[a[0]] << 8) | Fault_record_First2[a[1]];
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
	u16SciTemp = (Fault_record_First2[a[2]] << 8) | Fault_record_First2[a[3]];
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	for (j = 0; j < 4; j++)
	{
		k = FaultPoint_Second2 - 1 - j;
		if (k < 0)
		{
			k = Record_len + k;
		}
		a[j] = k;
	}
	u16SciTemp = (Fault_record_Second2[a[0]] << 8) | Fault_record_Second2[a[1]];
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
	u16SciTemp = (Fault_record_Second2[a[2]] << 8) | Fault_record_Second2[a[3]];
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	for (j = 0; j < 4; j++)
	{
		k = FaultPoint_Third2 - 1 - j;
		if (k < 0)
		{
			k = Record_len + k;
		}
		a[j] = k;
	}
	u16SciTemp = (Fault_record_Third2[a[0]] << 8) | Fault_record_Third2[a[1]];
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
	u16SciTemp = (Fault_record_Third2[a[2]] << 8) | Fault_record_Third2[a[3]];
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	for (j = 0; j < 12; j++)
	{ // 0xD002到这里。
		u16SciTemp = ((*(&System_ErrFlag.u8ErrFlag_Com_AFE1 + 2 * j)) << 8) | (*(&System_ErrFlag.u8ErrFlag_Com_AFE1 + 2 * j + 1));
		t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
		t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
	}

	switch (OPEN)
	{
	case 0:
		u16SciTemp = ((~((UINT16)(SystemStatus.all & 0x0000FFFF))) & 0x00FE) | (((UINT16)(SystemStatus.all & 0x0000FFFF)) & 0xFF01);
		break;
	case 1:
		u16SciTemp = (UINT16)(SystemStatus.all & 0x0000FFFF);
		break;
	default:
		u16SciTemp = (UINT16)(SystemStatus.all & 0x0000FFFF);
		break;
	}
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	u16SciTemp = (UINT16)(SystemStatus.all >> 16);
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	u16SciTemp = (UINT16)(System_OnOFF_Func.all & 0x0000FFFF);
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	u16SciTemp = (UINT16)(System_OnOFF_Func.all >> 16);
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	u16SciTemp = 0;
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	u16SciTemp = 0;
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	u16SciTemp = 0;
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	u16SciTemp = 0; // 可以加多一个
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	u16SciTemp = 0; // 可以加多一个
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	u16SciTemp = 0; // 可以加多一个
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	u16SciTemp = 0; // 可以加多一个
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	u16SciTemp = 0; // 可以加多一个
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	// 0xD200_1
	u16SciTemp = 0; // 可以加多一个
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
}

/*=================================================================
 * FUNCTION: Sci_Tx_RW_Fun
 * PURPOSE : 将需要发送的数据进行更新
 * INPUT:    void
 *
 * RETURN:   void
 *
 * CALLS:    void
 *
 * CALLED BY:Sci2_Updata()
 *
 *=================================================================*/
void Sci_ACK_0x03_RW_Data_Pro(struct RS485MSG *s, UINT8 t_u8BuffTemp[])
{ // 65个
	UINT16 u16SciTemp;
	UINT16 i, j;
	i = 0;
	for (j = 0; j < E2P_PARA_NUM_PROTECT; j++)
	{
		u16SciTemp = *(&PRT_E2ROMParas.u16VcellOvp_First + j);
		t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
		t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
	}
}

void Sci_ACK_0x03_RW_Data_Cali(struct RS485MSG *s, UINT8 t_u8BuffTemp[])
{ // 94个
	UINT16 u16SciTemp;
	UINT16 i, j;
	i = 0;
	for (j = 0; j < KB_NUM; j++)
	{
		u16SciTemp = g_u16CalibCoefK[j];
		t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
		t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
		u16SciTemp = g_i16CalibCoefB[j];
		t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
		t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
	}
}

void Sci_ACK_0x03_RW_Data_Other(struct RS485MSG *s, UINT8 t_u8BuffTemp[])
{ // 86
	UINT16 u16SciTemp;
	UINT16 i, j;
	i = 0;
	for (j = 0; j < SOC_TABLE_SIZE; j++)
	{ // 由于GetEndValue()函数的问题，只能混在一起
		switch (OtherElement.u16Soc_TableSelect)
		{
		case SOC_TABLE_TEST:
			u16SciTemp = SOC_Table_Set[j];
			break;
		case SOC_TABLE_LIFEPO:
			u16SciTemp = SOC_Table_LiFePO[j];
			break;
		case SOC_TABLE_TERNARYLI:
			u16SciTemp = SocTable_TernaryLi[j];
			break;
		case SOC_TABLE_LIFEPO2:
			// u16SciTemp = SocTable_LiFePO2[j];
			break;
		default:
			u16SciTemp = SOC_Table_Set[j];
			break;
		}
		// u16SciTemp = SOC_Table_Set[j];
		t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
		t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
	}

	for (j = 0; j < CompensateNUM; j++)
	{
		u16SciTemp = CopperLoss[j];
		t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
		t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
	}

	for (j = 0; j < CompensateNUM; j++)
	{
		u16SciTemp = CopperLoss_Num[j];
		t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
		t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
	}

	for (j = 0; j < E2P_PARA_NUM_RTC; j++)
	{
		// u16SciTemp = *(&RTC_time.RTC_Time_Year+j);
		u16SciTemp = 0;
		t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
		t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
	}
}

void Sci_ACK_0x03_RW_Data_OtherCanAdd(struct RS485MSG *s, UINT8 t_u8BuffTemp[])
{ // 32+24=56个
	UINT16 u16SciTemp;
	UINT16 i = 0, j;

	for (j = 0; j < E2P_PARA_NUM_OTHER_ELEMENT1; j++)
	{
		u16SciTemp = *(&OtherElement.u16Balance_OpenVoltage + j);
		t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
		t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
	}

	for (j = 0; j < E2P_PARA_NUM_HEAT_COOL; j++)
	{
		u16SciTemp = *(&Heat_Cool_Element.u16Heat_OpenTemp + j);
		// u16SciTemp = 0;
		t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
		t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
	}
}

void Sci_ACK_0x03(struct RS485MSG *s)
{
	UINT8 i;
	UINT16 u16SciTemp;
	if (s->AckType == RS485_ACK_POS)
	{
		if (s->u16RdRegStartAddrActure >= RS485_ADDR_RW_CALIB)
		{
			if (s->u16RdRegStartAddrActure >= RS485_ADDR_RO_START0)
			{
				Sci_ACK_0x03_ReadRegs_Data(s, g_u8SCITxBuff);
			}
			else if (s->u16RdRegStartAddrActure >= RS485_ADDR_RO_LCD)
			{
				Sci_ACK_0x03_ReadRegs_LCD(s, g_u8SCITxBuff);
			}
			else if (s->u16RdRegStartAddrActure >= RS485_ADDR_RW_OTHER_CANADD)
			{
				Sci_ACK_0x03_RW_Data_OtherCanAdd(s, g_u8SCITxBuff);
			}
			else if (s->u16RdRegStartAddrActure >= RS485_ADDR_RW_OTHER)
			{
				Sci_ACK_0x03_RW_Data_Other(s, g_u8SCITxBuff);
			}
			else if (s->u16RdRegStartAddrActure >= RS485_ADDR_RW_PORTECT)
			{
				Sci_ACK_0x03_RW_Data_Pro(s, g_u8SCITxBuff);
			}
			else
			{
				Sci_ACK_0x03_RW_Data_Cali(s, g_u8SCITxBuff);
			}
			// 头码，前三个字节保持不变
			s->u16Buffer[0] = (s->u16Buffer[0] != 0) ? RS485_SLAVE_ADDR : s->u16Buffer[0];
			s->u16Buffer[1] = s->enRs485CmdType;
			s->u16Buffer[2] = s->u16RdRegByteNum;
			// 数据
			for (i = 0; i < (s->u16RdRegByteNum); i++)
			{
				s->u16Buffer[i + 3] = g_u8SCITxBuff[i + ((s->u16RdRegStartAddr) << 1)];
			}
			i = s->u16RdRegByteNum + 3;
		}
	}
	else
	{
		i = 1;
		s->u16Buffer[i++] = s->enRs485CmdType | 0x80;
		s->u16Buffer[i++] = s->ErrorType;
	}
	u16SciTemp = Sci_CRC16RTU((UINT8 *)s->u16Buffer, i);
	s->u16Buffer[i++] = u16SciTemp & 0x00FF;
	s->u16Buffer[i++] = u16SciTemp >> 8;
	s->AckLenth = i;

	s->ptr_no = 0;
	s->csr = RS485_STA_TX_BUSY;
}

void Sci_ACK_0x06_0x10(struct RS485MSG *s)
{
	UINT8 i;
	UINT16 u16SciTemp;

	if (s->AckType == RS485_ACK_POS)
	{
		i = 6;
	}
	else
	{
		i = 1;
		s->u16Buffer[i++] = s->enRs485CmdType | 0x80;
		s->u16Buffer[i++] = s->ErrorType;
	}

	u16SciTemp = Sci_CRC16RTU((UINT8 *)s->u16Buffer, i);
	s->u16Buffer[i++] = u16SciTemp & 0x00FF;
	s->u16Buffer[i++] = u16SciTemp >> 8;
	s->AckLenth = i;

	s->ptr_no = 0;
	s->csr = RS485_STA_TX_BUSY;
}

static void Sci_FaultChk(SciPort *port)
{
    UINT32 errors = port->usart->ISR & (USART_ISR_ORE | USART_ISR_NE | USART_ISR_FE | USART_ISR_PE);
    if (errors)
    {
        /* 标准库USART_ClearFlag同样直接写ICR；禁止对清除寄存器做读改写。 */
        port->usart->ICR = errors;
        port->rxFault = 1;
    }
}

static void Sci_RxISR_Deal(SciPort *port)
{
    struct RS485MSG *s = port->msg;
    USART_TypeDef *usart = port->usart;
    UINT8 frameInvalid = 0;
    UINT16 expectedIndex = 0;

    usart->CR1 &= ~USART_CR1_RXNEIE;
    if (s->ptr_no >= RS485_MAX_BUFFER_SIZE)
    {
        (void)usart->RDR;
        Sci_ClearFrameState(s);
        port->rxFault = 0;
        port->rxTimeoutTick = 0;
        usart->CR1 |= USART_CR1_RE;
        usart->CR1 |= USART_CR1_RXNEIE;
        return;
    }

    s->u16Buffer[s->ptr_no] = (UINT8)usart->RDR;
    if (0 == s->ptr_no)
    {
        s->u8FrameDataLength = 0;
        s->u8FrameCommand = 0;
        s->u16FrameAddress = 0;
        if ((s->u16Buffer[0] == RS485_SLAVE_ADDR) || (s->u16Buffer[0] == RS485_BROADCAST_ADDR))
        {
            s->u8FrameProtocol = SCI_FRAME_PROTOCOL_MODBUS;
        }
        else if (port->supportsP12 && (s->u16Buffer[0] == P12_FRAME_HEAD0))
        {
            s->u8FrameProtocol = SCI_FRAME_PROTOCOL_P12;
        }
        else
        {
            frameInvalid = 1;
        }
    }
    else
    {
        switch (s->u8FrameProtocol)
        {
        case SCI_FRAME_PROTOCOL_MODBUS:
            if (1 == s->ptr_no)
            {
                switch (s->u16Buffer[1])
                {
                case RS485_CMD_READ_REGS:
                    s->enRs485CmdType = RS485_CMD_READ_REGS;
                    break;
                case RS485_CMD_WRITE_REG:
                    s->enRs485CmdType = RS485_CMD_WRITE_REG;
                    break;
                case RS485_CMD_WRITE_REGS:
                    s->enRs485CmdType = RS485_CMD_WRITE_REGS;
                    break;
                default:
                    frameInvalid = 1;
                    break;
                }
            }
            else
            {
                switch (s->enRs485CmdType)
                {
                case RS485_CMD_READ_REGS:
                case RS485_CMD_WRITE_REG:
                    if (7 == s->ptr_no)
                    {
                        s->csr = RS485_STA_RX_COMPLETE;
                    }
                    break;
                case RS485_CMD_WRITE_REGS:
                    if ((6 == s->ptr_no) && (!Sci_IsValidWriteRegsByteCount(s)))
                    {
                        frameInvalid = 1;
                    }
                    else if (s->ptr_no >= 7)
                    {
                        expectedIndex = (UINT16)s->u16Buffer[6] + 8u;
                        if (s->ptr_no == expectedIndex)
                        {
                            s->csr = RS485_STA_RX_COMPLETE;
                        }
                    }
                    break;
                default:
                    frameInvalid = 1;
                    break;
                }
            }
            break;

        case SCI_FRAME_PROTOCOL_P12:
            if ((1 == s->ptr_no) && (s->u16Buffer[1] != P12_FRAME_HEAD1))
            {
                frameInvalid = 1;
            }
            else if (3 == s->ptr_no)
            {
                s->u16FrameAddress = (UINT16)(s->u16Buffer[2] | (s->u16Buffer[3] << 8));
            }
            else if (4 == s->ptr_no)
            {
                s->u8FrameCommand = s->u16Buffer[4];
            }
            else if (5 == s->ptr_no)
            {
                s->u8FrameDataLength = s->u16Buffer[5];
                if (((UINT16)s->u8FrameDataLength + 8u) > RS485_MAX_BUFFER_SIZE)
                {
                    frameInvalid = 1;
                }
            }

            if (!frameInvalid && (s->ptr_no >= 5))
            {
                expectedIndex = (UINT16)s->u8FrameDataLength + 7u;
                if (s->ptr_no == expectedIndex)
                {
                    s->csr = RS485_STA_RX_COMPLETE;
                }
            }
            break;

        default:
            frameInvalid = 1;
            break;
        }
    }

    if (frameInvalid)
    {
        Sci_ClearFrameState(s);
        port->rxFault = 0;
        port->rxTimeoutTick = 0;
    }
    else
    {
        port->rxTimeoutTick = 0;
        s->ptr_no++;
        if ((RS485_STA_RX_COMPLETE != s->csr) && (s->ptr_no >= RS485_MAX_BUFFER_SIZE))
        {
            Sci_ClearFrameState(s);
            port->rxFault = 0;
            port->rxTimeoutTick = 0;
        }
    }

    if (RS485_STA_RX_COMPLETE == s->csr)
    {
        usart->CR1 &= ~(USART_CR1_RE | USART_CR1_RXNEIE);
    }
    else
    {
        usart->CR1 |= USART_CR1_RE;
        usart->CR1 |= USART_CR1_RXNEIE;
    }
}

/* 协议业务入口：仅主循环调用，新增协议必须显式增加分支。 */
static void Sci_ProcessRequest(SciPort *port)
{
    struct RS485MSG *s = port->msg;
    switch (s->u8FrameProtocol)
    {
    case SCI_FRAME_PROTOCOL_P12:
        if (P12_VerifyFrame(s) && P12_BuildResponse(s))
        {
            s->csr = RS485_STA_RX_OK;
            Sci_RS485ValidRequest(port);
        }
        else
            Sci_ResetFrameState(port);
        break;

    case SCI_FRAME_PROTOCOL_MODBUS:
        CRC_verify(s);
        if (s->AckType == RS485_ACK_POS)
        {
            switch (s->enRs485CmdType)
            {
            case RS485_CMD_READ_REGS:
                Sci_Deal_ReadRegs_0x03(s);
                break;
            case RS485_CMD_WRITE_REG:
                Sci_Deal_WrReg_0x06(s);
                break;
            case RS485_CMD_WRITE_REGS:
                Sci_Deal_WrRegs_0x10(s);
                break;
            default:
                s->u16RdRegByteNum = 0;
                s->AckType = RS485_ACK_NEG;
                s->ErrorType = RS485_ERROR_NULL;
                break;
            }
        }
        if (s->AckType == RS485_ACK_POS)
            Sci_RS485ValidRequest(port);
        s->csr = RS485_STA_RX_OK;
        break;

    default:
        Sci_ResetFrameState(port);
        break;
    }
    port->rxTimeoutTick = 0;
}

/* 协议应答入口：传输层只关心缓冲和长度，不理解命令格式。 */
static UINT8 Sci_PrepareResponse(struct RS485MSG *s)
{
    switch (s->u8FrameProtocol)
    {
    case SCI_FRAME_PROTOCOL_P12:
        return 1; /* P12_BuildResponse已在业务处理阶段生成整帧。 */
    case SCI_FRAME_PROTOCOL_MODBUS:
        switch (s->enRs485CmdType)
        {
        case RS485_CMD_READ_REGS:
            Sci_ACK_0x03(s);
            return 1;
        case RS485_CMD_WRITE_REG:
        case RS485_CMD_WRITE_REGS:
            Sci_ACK_0x06_0x10(s);
            return 1;
        default:
            return 0;
        }
    default:
        return 0;
    }
}

static void Sci_Service(SciPort *port)
{
    struct RS485MSG *s = port->msg;
    USART_TypeDef *usart = port->usart;
    UINT32 primask = __get_PRIMASK();
    /* 防止RX中断在超时判断后发布完整帧，又被主循环清空。 */
    __disable_irq();
    if ((RS485_STA_IDLE == s->csr) && (s->ptr_no > 0) && g_st_SysTimeFlag.bits.b1Sys10msFlag1)
    {
        if (++port->rxTimeoutTick >= SCI_RX_TIMEOUT_TICKS)
        {
            Sci_ResetFrameState(port);
        }
    }
    __set_PRIMASK(primask);
    switch (s->csr)
    {
    case RS485_STA_IDLE:
    {
        break;
    }

    case RS485_STA_RX_COMPLETE:
    {
        usart->CR1 &= ~USART_CR1_RXNEIE;
        if (port->rxFault)
        {
            Sci_ResetFrameState(port);
            break;
        }
        Sci_ProcessRequest(port);
        break;
    }

    case RS485_STA_RX_OK:
    {
        if (Sci_PrepareResponse(s))
            Sci_StartTx(port);
        else
            Sci_ResetFrameState(port);
        break;
    }

    case RS485_STA_TX_BUSY:
    {
        break;
    }

    default:
    {
        Sci_ResetFrameState(port);
        break;
    }
    }
}

#ifdef _COMMOM_UPPER_SCI1
void InitSCI1_CommonUpper(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE); // 开启USART1外设时钟
	// RCC->AHBENR |= 1<<17;										//开启GPIOA的外设时钟

	// Enable the USART1 Interrupt(使能USART1中断)
	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	// USART1_TX -> PA9 , USART1_RX -> PA10
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_1); // 030的AF表格在非reg的datasheet里
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_1);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	// 串口初始化
	USART_InitStructure.USART_BaudRate = 19200;										// 设置串口波特率
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;						// 设置数据位
	USART_InitStructure.USART_StopBits = USART_StopBits_1;							// 设置停止位
	USART_InitStructure.USART_Parity = USART_Parity_No;								// 设置效验位
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // 设置流控制
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;					// 设置工作模式
	USART_Init(USART1, &USART_InitStructure);										// 配置入结构体

	USART1->CR3 |= USART_CR3_EIE;	// EIE，开帧错误中断，同时开启噪声中断
	USART1->CR3 |= USART_CR3_ONEBIT; // 未被使能前改写，禁止噪声中断

	Sci_DataInit(sci1.msg);
	sci1.rxFault = 0;
	sci1.rxTimeoutTick = 0;
	sci1.txTimeoutTick = 0;
	Sci_SetTxDirection(&sci1, 0);
	USART_Cmd(USART1, ENABLE);					   // 使能串口1
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE); // 使能接收中断


}

void Sci1_CommonUpper_FaultChk(void)
{
    Sci_FaultChk(&sci1);
}

void Sci1_CommonUpper_Rx_Deal(void)
{
    Sci_RxISR_Deal(&sci1);
}

void Sci1_CommonUpper_Tx_Deal(void)
{
    Sci_TxISR_Deal(&sci1);
}
#endif

#ifdef _COMMOM_UPPER_SCI2
void InitSCI2_CommonUpper(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
	// RCC->AHBENR |= 1<<17;										//开启GPIOA的外设时钟

	// Enable the USART2 Interrupt(使能USART2中断)
	NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	// USART2_TX -> PA2 , USART2_RX -> PA3
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_1); // 030的AF表格在非reg的datasheet里
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_1);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	// 串口初始化
	USART_InitStructure.USART_BaudRate = 115200;									// 设置串口波特率
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;						// 设置数据位
	USART_InitStructure.USART_StopBits = USART_StopBits_1;							// 设置停止位
	USART_InitStructure.USART_Parity = USART_Parity_No;								// 设置效验位
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // 设置流控制
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;					// 设置工作模式
	USART_Init(USART2, &USART_InitStructure);										// 配置入结构体

	USART2->CR3 |= USART_CR3_EIE;	// EIE，开帧错误中断，同时开启噪声中断
	USART2->CR3 |= USART_CR3_ONEBIT; // 未被使能前改写，禁止噪声中断

	Sci_DataInit(sci2.msg);
	sci2.rxFault = 0;
	sci2.rxTimeoutTick = 0;
	sci2.txTimeoutTick = 0;
	Sci_SetTxDirection(&sci2, 0);
	USART_Cmd(USART2, ENABLE);					   // 使能串口1
	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE); // 使能接收中断


}

void Sci2_CommonUpper_FaultChk(void)
{
    Sci_FaultChk(&sci2);
}

void Sci2_CommonUpper_Rx_Deal(void)
{
    Sci_RxISR_Deal(&sci2);
}

void Sci2_CommonUpper_Tx_Deal(void)
{
    Sci_TxISR_Deal(&sci2);
}
#endif

void Sci_WrRegs_0x10_CalibCoef(UINT16 u16Channel, struct RS485MSG *s)
{
	UINT16 t_u16K, t_u16B, t_u16Temp;
	INT16 t_i16B;
	UINT16 u16WrRegNum;
	u16WrRegNum = s->u16Buffer[5] + (s->u16Buffer[4] << 8);

	if (u16WrRegNum == 2)
	{
		t_u16K = s->u16Buffer[8] + (s->u16Buffer[7] << 8);
		t_u16B = s->u16Buffer[10] + (s->u16Buffer[9] << 8);

		t_u16Temp = t_u16B & 0x8000;
		if (t_u16Temp == 0)
		{
			t_i16B = t_u16B & 0x7FFF;
		}
		else
		{
			t_i16B = -(t_u16B & 0x7FFF);
		}

		if ((t_u16K < SYSKMIN) || (t_u16K > SYSKMAX))
		{
			s->AckType = RS485_ACK_NEG;
			s->ErrorType = RS485_ERROR_DATA_INVALID;
			return;
		}

		if ((t_i16B < SYSBMIN) || (t_i16B > SYSBMAX))
		{
			s->AckType = RS485_ACK_NEG;
			s->ErrorType = RS485_ERROR_DATA_INVALID;
			return;
		}

		t_u16Temp = (u16Channel - RS485_CMD_ADDR_VC1CALIB_K) >> 1;
		g_u16CalibCoefK[t_u16Temp] = t_u16K;
		g_i16CalibCoefB[t_u16Temp] = t_i16B;
		u8E2P_KB_WriteFlag = 1;
		u8E2P_KB_WritePos = t_u16Temp;
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_CMD_INVALID;
	}
}

// 节省了很多代码量吧？
void Sci_WrRegs_0x10_Protect(UINT16 u16Channel, struct RS485MSG *s)
{
	UINT16 t_u16Temp, i;
	UINT16 u16WrRegNum;
	u16WrRegNum = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (u16WrRegNum == 5)
	{
		t_u16Temp = u16Channel - RS485_CMD_ADDR_VCELL_OVP_FIRST;
		for (i = 0; i < 5; ++i)
		{
			*(&PRT_E2ROMParas.u16VcellOvp_First + i + t_u16Temp) = (UINT16)(s->u16Buffer[2 * i + 8] + (s->u16Buffer[2 * i + 7] << 8));
		}

		if (u16Channel >= RS485_CMD_ADDR_VDELTA_OP_FIRST)
		{
			u32E2P_Pro_Other_WriteFlag = (EE_FLAG_VCELL_OVP_FIRST | EE_FLAG_VCELL_OVP_SECOND | EE_FLAG_VCELL_OVP_THIRD | EE_FLAG_VCELL_OVP_RCV | EE_FLAG_VCELL_OVP_FILTER)
										 << (t_u16Temp - E2P_PARA_NUM_VOLCUR_PROTECT - E2P_PARA_NUM_TEM_PROTECT);
		}
		else if (u16Channel >= RS485_CMD_ADDR_TCHG_OTP_FIRST)
		{
			u32E2P_Pro_Temp_WriteFlag = (EE_FLAG_VCELL_OVP_FIRST | EE_FLAG_VCELL_OVP_SECOND | EE_FLAG_VCELL_OVP_THIRD | EE_FLAG_VCELL_OVP_RCV | EE_FLAG_VCELL_OVP_FILTER)
										<< (t_u16Temp - E2P_PARA_NUM_VOLCUR_PROTECT);
		}
		else
		{
			u32E2P_Pro_VolCur_WriteFlag = (EE_FLAG_VCELL_OVP_FIRST | EE_FLAG_VCELL_OVP_SECOND | EE_FLAG_VCELL_OVP_THIRD | EE_FLAG_VCELL_OVP_RCV | EE_FLAG_VCELL_OVP_FILTER) << (t_u16Temp);
			// InitData_SOC();
		}
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_CMD_INVALID;
	}
}

// 这种写法其实也有问题，主要是，倘若写失败，但是上传上位机是修改成功，就尴尬
// 但是上位机会有EEPROM写失败标志位弥补
void Sci_WrRegs_0x10_SocTable(struct RS485MSG *s)
{
}

void Sci_WrRegs_0x10_CopperLoss(struct RS485MSG *s)
{
}

void Sci_WrRegs_0x10_RTC(struct RS485MSG *s)
{
}

void Sci_WrRegs_0x10_Balance(struct RS485MSG *s)
{
	UINT8 i;
	UINT16 u16WrRegNum;
	u16WrRegNum = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (u16WrRegNum == 8)
	{
		for (i = 0; i < 8; ++i)
		{
			*(&OtherElement.u16Balance_OpenVoltage + i) = (UINT16)(s->u16Buffer[2 * i + 8] + (s->u16Buffer[2 * i + 7] << 8));
		}
		u32E2P_OtherElement1_WriteFlag |= EE_FLAG_OTHER1_BALANCE_OV;
		u32E2P_OtherElement1_WriteFlag |= EE_FLAG_OTHER1_BALANCE_OW;
		u32E2P_OtherElement1_WriteFlag |= EE_FLAG_OTHER1_BALANCE_CW1;
		u32E2P_OtherElement1_WriteFlag |= EE_FLAG_OTHER1_BALANCE_CW2;
		u32E2P_OtherElement1_WriteFlag |= EE_FLAG_OTHER1_OPENTIME_ODD;
		u32E2P_OtherElement1_WriteFlag |= EE_FLAG_OTHER1_OPENTIME_EVEN;
		u32E2P_OtherElement1_WriteFlag |= EE_FLAG_OTHER1_OPENTIME_MOS;
		u32E2P_OtherElement1_WriteFlag |= EE_FLAG_OTHER1_RES;
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_CMD_INVALID;
	}
}

void Sci_WrRegs_0x10_SysOther(struct RS485MSG *s)
{
	UINT8 i;
	UINT16 u16WrRegNum;
	u16WrRegNum = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (u16WrRegNum == 8)
	{
		for (i = 0; i < 8; ++i)
		{
			*(&OtherElement.u16CS_Cur_CHGmax + i) = (UINT16)(s->u16Buffer[2 * i + 8] + (s->u16Buffer[2 * i + 7] << 8));
		}
		u32E2P_OtherElement1_WriteFlag |= EE_FLAG_CS_CUR_CHGMAX;
		u32E2P_OtherElement1_WriteFlag |= EE_FLAG_CS_CUR_DSGMAX;
		u32E2P_OtherElement1_WriteFlag |= EE_FLAG_CBC_CUR_CHG;
		u32E2P_OtherElement1_WriteFlag |= EE_FLAG_CBC_CUR_DSG;
		// u32E2P_OtherElement1_WriteFlag |= EE_FLAG_OTHER1_COOL_DSG_H;		//不保存
		// u32E2P_OtherElement1_WriteFlag |= EE_FLAG_OTHER1_COOL_DSG_L;
		// u32E2P_OtherElement1_WriteFlag |= EE_FLAG_OTHER1_COOL_CHG_H;
		// u32E2P_OtherElement1_WriteFlag |= EE_FLAG_OTHER1_COOL_CHG_L;

		InitShortCur();
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_CMD_INVALID;
	}
}

void Sci_WrRegs_0x10_SleepElement(struct RS485MSG *s)
{
	UINT8 i;
	UINT16 u16WrRegNum;
	u16WrRegNum = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (u16WrRegNum == 8)
	{
		for (i = 0; i < 8; ++i)
		{
			*(&OtherElement.u16Sleep_VNormal + i) = (UINT16)(s->u16Buffer[2 * i + 8] + (s->u16Buffer[2 * i + 7] << 8));
		}
		u32E2P_OtherElement1_WriteFlag |= EE_FLAG_OTHER1_SLEEP_V_NORMAL;
		u32E2P_OtherElement1_WriteFlag |= EE_FLAG_OTHER1_SLEEP_TIME_NORMAL;
		u32E2P_OtherElement1_WriteFlag |= EE_FLAG_OTHER1_SLEEP_V_LOW;
		u32E2P_OtherElement1_WriteFlag |= EE_FLAG_OTHER1_SLEEP_TIME_LOW;
		u32E2P_OtherElement1_WriteFlag |= EE_FLAG_OTHER1_SLEEP_I_CHG;
		u32E2P_OtherElement1_WriteFlag |= EE_FLAG_OTHER1_SLEEP_I_DSG;
		u32E2P_OtherElement1_WriteFlag |= EE_FLAG_OTHER1_SLEEP_RES1;
		u32E2P_OtherElement1_WriteFlag |= EE_FLAG_OTHER1_SLEEP_RES2;

		reset_sleep_state = 1;
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_CMD_INVALID;
	}
}

void Sci_WrRegs_0x10_SocElement(struct RS485MSG *s)
{
	UINT8 i;
	UINT16 u16WrRegNum;
	u16WrRegNum = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (u16WrRegNum == 4)
	{
		for (i = 0; i < 4; ++i)
		{
			*(&OtherElement.u16Soc_Ah + i) = (UINT16)(s->u16Buffer[2 * i + 8] + (s->u16Buffer[2 * i + 7] << 8));
		}
		u32E2P_OtherElement1_WriteFlag |= EE_FLAG_OTHER1_SOC_AH;
		u32E2P_OtherElement1_WriteFlag |= EE_FLAG_OTHER1_SOC_CYCLE_TIME;
		u32E2P_OtherElement1_WriteFlag |= EE_FLAG_OTHER1_SOC_RES1;
		u32E2P_OtherElement1_WriteFlag |= EE_FLAG_OTHER1_SOC_RES2;

		InitData_SOC();
		SOC_Enhance_Element.u16_RefreshData_Flag = 2;
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_CMD_INVALID;
	}
}

void Sci_WrRegs_0x10_SystemElement(struct RS485MSG *s)
{
	UINT8 i;
	UINT16 u16WrRegNum;
	u16WrRegNum = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (u16WrRegNum == 4)
	{
		for (i = 0; i < 4; ++i)
		{
			*(&OtherElement.u16Sys_SeriesNum + i) = (UINT16)(s->u16Buffer[2 * i + 8] + (s->u16Buffer[2 * i + 7] << 8));
		}
		u32E2P_OtherElement1_WriteFlag |= EE_FLAG_OTHER1_SYS_SERIES_NUM;
		u32E2P_OtherElement1_WriteFlag |= EE_FLAG_OTHER1_SYS_CS_RESIS;
		u32E2P_OtherElement1_WriteFlag |= EE_FLAG_OTHER1_SYS_CS_NUM;
		u32E2P_OtherElement1_WriteFlag |= EE_FLAG_OTHER1_SYS_PRECHG_TIME;
		SeriesNum = OtherElement.u16Sys_SeriesNum;
		// CS，直接使用不需要再赋值，TODO
		// 还是赋值吧，提高效率
		g_u32CS_Res_AFE = ((UINT32)OtherElement.u16Sys_CS_Res_Num * 844 << 10) / OtherElement.u16Sys_CS_Res / 100;
		InitData_Drivers();
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_CMD_INVALID;
	}
}

void Sci_WrRegs_0x10_HeatCoolElement(struct RS485MSG *s)
{
	UINT8 i;
	UINT16 u16WrRegNum;
	u16WrRegNum = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (u16WrRegNum == E2P_PARA_NUM_HEAT_COOL)
	{
		for (i = 0; i < E2P_PARA_NUM_HEAT_COOL; ++i)
		{
			*(&Heat_Cool_Element.u16Heat_OpenTemp + i) = (UINT16)(s->u16Buffer[2 * i + 8] + (s->u16Buffer[2 * i + 7] << 8));
		}
		u32E2P_HeatCool_WriteFlag |= E2P_PARA_ALL_HEAT_COOL_ELE;
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_CMD_INVALID;
	}
}

void Sci_WrRegs_0x10_FlashConnect(struct RS485MSG *s)
{
	UINT16 u16WrRegNum;
	u16WrRegNum = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (u16WrRegNum == 1)
	{
		if (FLASH_COMPLETE != FlashWriteOneHalfWord(FLASH_ADDR_UPDATE_FLAG, FLASH_TO_IAP_VALUE))
		{
			// System_ERROR_UserCallback(ERROR_FLASH);
			s->AckType = RS485_ACK_NEG;
			s->ErrorType = RS485_ERROR_CMD_INVALID;
		}
		else
		{
			u8FlashUpdateE2PROM = 1;
		}
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_CMD_INVALID;
	}
}

/* 把BMS序列号，硬件版本号， 软件版本号写入 ohterInfor结构体
 * 并把写入到EEPROM标志置位
 * startADDR  如起始地址
 */
void Sci_WrRegs_0x10_SN_Version(UINT16 startADDR, struct RS485MSG *s)
{
	UINT8 i;
	UINT16 u16WrSNlength;

	u16WrSNlength = (UINT16)((UINT16)s->u16Buffer[5] + ((UINT16)s->u16Buffer[4] << 8)) << 1;

	switch (startADDR - RS485_ADDR_SN_SERIAL_NUM)
	{
	case 0:
		for (i = 0; i < PRODUCT_ID_LENGTH_MAX; ++i)
		{
			if (i < u16WrSNlength)
			{
				ProductionInfor.BMS_SerialNumber[i] = s->u16Buffer[7 + i];
			}
			else
			{
				ProductionInfor.BMS_SerialNumber[i] = '\0';
			}
		}
		ProductionInfor.BMS_SerialNumberLength = u16WrSNlength;
		ProductionInfor.BMS_SerialNumber_WriteFlag = 1;
		break;

	case 1:
		for (i = 0; i < PRODUCT_ID_LENGTH_MAX; ++i)
		{
			if (i < u16WrSNlength)
			{
				ProductionInfor.BMS_HardWareVersion[i] = s->u16Buffer[7 + i];
			}
			else
			{
				ProductionInfor.BMS_HardWareVersion[i] = '\0';
			}
		}
		ProductionInfor.BMS_HardWareVersionLength = u16WrSNlength;
		ProductionInfor.BMS_HardWareVersion_WriteFlag = 1;
		break;

	case 2:
		for (i = 0; i < PRODUCT_ID_LENGTH_MAX; ++i)
		{
			if (i < u16WrSNlength)
			{
				ProductionInfor.BMS_SoftWareVersion[i] = s->u16Buffer[7 + i];
			}
			else
			{
				ProductionInfor.BMS_SoftWareVersion[i] = '\0';
			}
		}
		ProductionInfor.BMS_SoftWareVersionLength = u16WrSNlength;
		ProductionInfor.BMS_SoftWareVersion_WriteFlag = 1;
		break;

	default:
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_CMD_INVALID;
		break;
	}
}

void Sci_WrReg_0x06_Reset_CalibCoef(struct RS485MSG *s)
{
	UINT8 i;
	switch (s->u16Buffer[5] + (s->u16Buffer[4] << 8))
	{
	case 0x55AA:
		for (i = 0; i < 32; i++)
		{
			g_u16CalibCoefK[i] = SYSKDEFAULT;
			g_i16CalibCoefB[i] = SYSBDEFAULT;
			WriteEEPROM_Word_NoZone((E2P_ADDR_START_CALIB_K + (i << 1)), g_u16CalibCoefK[i]);
			WriteEEPROM_Word_NoZone((E2P_ADDR_START_CALIB_B + (i << 1)), g_i16CalibCoefB[i]);
		}
		break;
	case 0x55AB:

		g_u16CalibCoefK[VOLT_AFE1] = SYSKDEFAULT;
		g_i16CalibCoefB[VOLT_AFE1] = SYSBDEFAULT;
		WriteEEPROM_Word_NoZone((E2P_ADDR_START_CALIB_K + (VOLT_AFE1 << 1)), g_u16CalibCoefK[VOLT_AFE1]);
		WriteEEPROM_Word_NoZone((E2P_ADDR_START_CALIB_B + (VOLT_AFE1 << 1)), g_i16CalibCoefB[VOLT_AFE1]);
		break;
	case 0x55AC:
		g_u16CalibCoefK[VOLT_AFE2] = SYSKDEFAULT;
		g_i16CalibCoefB[VOLT_AFE2] = SYSBDEFAULT;
		WriteEEPROM_Word_NoZone((E2P_ADDR_START_CALIB_K + (VOLT_AFE2 << 1)), g_u16CalibCoefK[VOLT_AFE2]);
		WriteEEPROM_Word_NoZone((E2P_ADDR_START_CALIB_B + (VOLT_AFE2 << 1)), g_i16CalibCoefB[VOLT_AFE2]);
		break;
	case 0x55AD:
		g_u16CalibCoefK[VOLT_VBUS] = SYSKDEFAULT;
		g_i16CalibCoefB[VOLT_VBUS] = SYSBDEFAULT;
		WriteEEPROM_Word_NoZone((E2P_ADDR_START_CALIB_K + (VOLT_VBUS << 1)), g_u16CalibCoefK[VOLT_VBUS]);
		WriteEEPROM_Word_NoZone((E2P_ADDR_START_CALIB_B + (VOLT_VBUS << 1)), g_i16CalibCoefB[VOLT_VBUS]);
		break;
	case 0x55AE:
		for (i = 0; i < 10; i++)
		{
			g_u16CalibCoefK[MDL_TEMP1 + i] = SYSKDEFAULT;
			g_i16CalibCoefB[MDL_TEMP1 + i] = SYSBDEFAULT;
			WriteEEPROM_Word_NoZone((E2P_ADDR_START_CALIB_K + ((MDL_TEMP1 + i) << 1)), g_u16CalibCoefK[i]);
			WriteEEPROM_Word_NoZone((E2P_ADDR_START_CALIB_B + ((MDL_TEMP1 + i) << 1)), g_i16CalibCoefB[i]);
		}
		break;
	case 0x55AF:
		g_u16CalibCoefK[MDL_IDSG] = SYSKDEFAULT;
		g_i16CalibCoefB[MDL_IDSG] = SYSBDEFAULT;
		WriteEEPROM_Word_NoZone((E2P_ADDR_START_CALIB_K + (MDL_IDSG << 1)), g_u16CalibCoefK[MDL_IDSG]);
		WriteEEPROM_Word_NoZone((E2P_ADDR_START_CALIB_B + (MDL_IDSG << 1)), g_i16CalibCoefB[MDL_IDSG]);
		break;
	case 0x55B0:
		g_u16CalibCoefK[MDL_ICHG] = SYSKDEFAULT;
		g_i16CalibCoefB[MDL_ICHG] = SYSBDEFAULT;
		WriteEEPROM_Word_NoZone((E2P_ADDR_START_CALIB_K + (MDL_ICHG << 1)), g_u16CalibCoefK[MDL_ICHG]);
		WriteEEPROM_Word_NoZone((E2P_ADDR_START_CALIB_B + (MDL_ICHG << 1)), g_i16CalibCoefB[MDL_ICHG]);
		break;
	default:
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_DATA_INVALID;
		break;
	}
}

void Sci_WrReg_0x06_Reset_ProtectRecord(struct RS485MSG *s)
{
	UINT16 u16SciRegData;
	UINT8 i;
	u16SciRegData = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (0x0001 == u16SciRegData)
	{
		for (i = 0; i < Record_len; ++i)
		{
			Fault_record_First2[i] = 0;
			Fault_record_Second2[i] = 0;
			Fault_record_Third2[i] = 0;
		}
		FaultPoint_First2 = 0;
		FaultPoint_Second2 = 0;
		FaultPoint_Third2 = 0;
		Fault_Flag_Fisrt.all = 0;
		Fault_Flag_Second.all = 0;
		Fault_Flag_Third.all = 0;
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_DATA_INVALID;
	}
}

void Sci_WrReg_0x06_Reset_ProtectElement(struct RS485MSG *s)
{
	UINT16 u16SciRegData;
	UINT8 i;
	const struct PRT_E2ROM_PARAS PrtE2PARAS_Default = E2P_PROTECT_DEFAULT_PRT;
	u16SciRegData = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (0x0001 == u16SciRegData)
	{
		for (i = 0; i < E2P_PARA_NUM_PROTECT; ++i)
		{
			*(&PRT_E2ROMParas.u16VcellOvp_First + i) = *(&PrtE2PARAS_Default.u16VcellOvp_First + i);
		}
		u32E2P_Pro_VolCur_WriteFlag = E2P_PARA_ALL_VOLCUR_PROTECT;
		u32E2P_Pro_Temp_WriteFlag = E2P_PARA_ALL_TEM_PROTECT;
		u32E2P_Pro_Other_WriteFlag = E2P_PARA_ALL_OTHER_PROTECT;
		// InitData_SOC();
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_DATA_INVALID;
	}
}

void Sci_WrReg_0x06_Reset_OtherCanAdd(struct RS485MSG *s)
{
	UINT16 u16SciRegData;
	UINT8 i;
	const struct OTHER_ELEMENT OtherElement_Default = OtherElement_default;
	u16SciRegData = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (0x0001 == u16SciRegData)
	{
		for (i = 0; i < E2P_PARA_NUM_OTHER_ELEMENT1; ++i)
		{
			*(&OtherElement.u16Balance_OpenVoltage + i) = *(&OtherElement_Default.u16Balance_OpenVoltage + i);
		}
		u32E2P_OtherElement1_WriteFlag = E2P_PARA_ALL_OTHER_ELEMENT1;
		SeriesNum = OtherElement.u16Sys_SeriesNum;
		g_u32CS_Res_AFE = ((UINT32)OtherElement.u16Sys_CS_Res_Num * 844 << 10) / OtherElement.u16Sys_CS_Res / 100;

		InitData_SOC();
		// 同步更新安时数，循环次数等
		SOC_Enhance_Element.u16_RefreshData_Flag = 2;
		InitData_Drivers();

		InitShortCur();

		reset_sleep_state = 1;
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_DATA_INVALID;
	}
}

void Sci_WrReg_0x06_Reset_HeatCool(struct RS485MSG *s)
{
	UINT16 u16SciRegData;
	UINT8 i;
	const struct HEAT_COOL_ELEMENT HeatCoolEle_Default = HeatCoolElement_Default;

	u16SciRegData = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (0x0001 == u16SciRegData)
	{
		for (i = 0; i < E2P_PARA_NUM_HEAT_COOL; ++i)
		{
			*(&Heat_Cool_Element.u16Heat_OpenTemp + i) = *(&HeatCoolEle_Default.u16Heat_OpenTemp + i);
		}
		u32E2P_HeatCool_WriteFlag = E2P_PARA_ALL_HEAT_COOL_ELE;
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_DATA_INVALID;
	}
}

void Sci_WrReg_0x06_SwitchON(struct RS485MSG *s)
{
}

void Sci_WrReg_0x06_SwitchOFF(struct RS485MSG *s)
{
}

// 关于这个函数
// A:第一次打开这个功能，以前从来没打开过，则因为各种标志位变量都没变过(switch结构里面的)，所以会进行初始化验证
// B:其中关闭了，又打开，则已经初始化过一次，这次打开就继续按照上一次的进度继续下去
void Sci_WrReg_0x06_BMS_FunctionON(struct RS485MSG *s)
{
	UINT16 u16SciRegData;
	u16SciRegData = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (u16SciRegData >= 1 && u16SciRegData <= 32)
	{
		switch (u16SciRegData)
		{		// 如果是以下功能被打开，则需要初始化验证，别的功能直接关就好
		case 1: // 均衡
			if (!System_OnOFF_Func_StartUpRec.bits.b1OnOFF_Balance)
			{
				System_OnOFF_Func_StartUpRec.bits.b1OnOFF_Balance = 1;
				System_Func_StartUp.bits.b1StartUpFlag_Balance = 1;
			}
			break;

		case 3: // MOS或者接触器功能
			if (!System_OnOFF_Func_StartUpRec.bits.b1OnOFF_MOS_Relay)
			{
				System_OnOFF_Func_StartUpRec.bits.b1OnOFF_MOS_Relay = 1;
				System_Func_StartUp.bits.b1StartUpFlag_MOS = 1;
				System_Func_StartUp.bits.b1StartUpFlag_Relay = 1;
			}
			break;

		case 6: // 加热功能
			if (!System_OnOFF_Func_StartUpRec.bits.b1OnOFF_Heat)
			{
				System_OnOFF_Func_StartUpRec.bits.b1OnOFF_Heat = 1;
				System_Func_StartUp.bits.b1StartUpFlag_Heat = 1;
			}
			break;

		case 7: // 冷凝功能
			if (!System_OnOFF_Func_StartUpRec.bits.b1OnOFF_Cool)
			{
				System_OnOFF_Func_StartUpRec.bits.b1OnOFF_Cool = 1;
				System_Func_StartUp.bits.b1StartUpFlag_Cool = 1;
			}
			break;

		case 8: // 激活模拟前端AFE1
			App_WakeUpAFE();
			InitialisebqMaximo(DEVICE_ADDR_AFE1);
			break;

		case 0x0A: // 立刻进入休眠
			Sleep_Mode.bits.b1ForceToSleep_L3 = 1;
			break;

		default:
			break;
		}

		System_OnOFF_Func.all |= ((UINT32)1 << (u16SciRegData - 1));
		if (u16SciRegData == 0x0B)
		{
			// System_OnOFF_Func.bits.b1OnOFF_SOC_Zero
			// 默认为0，不需要保存
		}
		else
		{
			WriteEEPROM_Word_NoZone(EEPROM_ADDR_SYS_FUNC_SELECT, (UINT16)(System_OnOFF_Func.all & 0x0000FFFF));
			WriteEEPROM_Word_NoZone(EEPROM_ADDR_SYS_FUNC_SELECT + 2, (UINT16)(System_OnOFF_Func.all >> 16));
		}

		if (System_OnOFF_Func.bits.b1OnOFF_SOC_Fixed)
		{
			SOC_Enhance_Element.u16_RefreshData_Flag = 1;
		}
		if (System_OnOFF_Func.bits.b1OnOFF_SOC_Zero)
		{
			SOC_Enhance_Element.u16_RefreshData_Flag = 2;
		}
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_DATA_INVALID;
	}
}

void Sci_WrReg_0x06_BMS_FunctionOFF(struct RS485MSG *s)
{
	UINT16 u16SciRegData;
	u16SciRegData = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (u16SciRegData >= 1 && u16SciRegData <= 32)
	{
		//*(&System_OnOFF_Func.bits.b1OnOFF_Balance+(u16SciRegData-1)) = 0;
		System_OnOFF_Func.all &= ~((UINT32)1 << (u16SciRegData - 1)); // 功能途中关闭不需要初始化验证

		if (u16SciRegData == 0x0B)
		{
			// System_OnOFF_Func.bits.b1OnOFF_SOC_Zero
			// 默认为0，不需要保存
		}
		else
		{
			WriteEEPROM_Word_NoZone(EEPROM_ADDR_SYS_FUNC_SELECT, (UINT16)(System_OnOFF_Func.all & 0x0000FFFF));
			WriteEEPROM_Word_NoZone(EEPROM_ADDR_SYS_FUNC_SELECT + 2, (UINT16)(System_OnOFF_Func.all >> 16));
		}
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_DATA_INVALID;
	}
}

void Sci_WrReg_0x06_SetSocOnce(struct RS485MSG *s)
{
	UINT16 u16SciRegData;
	u16SciRegData = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (u16SciRegData <= 100)
	{
		SOC_Enhance_Element.u16_RefreshData_Flag = 3;
		SOC_Enhance_Element.u8_SetSocOnce = u16SciRegData;
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_DATA_INVALID;
	}
}

void InitUSART_CommonUpper(void)
{
#ifdef _COMMOM_UPPER_SCI1
	InitSCI1_CommonUpper();
#endif

#ifdef _COMMOM_UPPER_SCI2
	InitSCI2_CommonUpper();
    USART2->CR1 &= ~(USART_CR1_RE | USART_CR1_RXNEIE);
    s_rs485Ready = 1;
    Sci_RS485WakeInit();
#endif
}

void App_CommonUpper(void)
{
#ifdef _COMMOM_UPPER_SCI1
	Sci_Service(&sci1);
#endif

#ifdef _COMMOM_UPPER_SCI2
    if (s_rs485PowerOn)
        Sci_Service(&sci2);
    Sci_RS485PowerService();
#endif
}

#if 1
#define debug_uart USART1
#else
#define debug_uart USART2
#endif

int fputc(int ch, FILE *f)
{
#if 0 /* 将需要printf的字符通过串口中断FIFO发送出去，printf函数会立即返回 */
	comSendChar(COM1, ch);

	return ch;
#else /* 采用阻塞方式发送每个字符,等待数据发送完毕 */
	/* 写一个字节到USART1 */
	USART_SendData(debug_uart, (uint8_t)ch);

	/* 等待发送结束 */
	while (USART_GetFlagStatus(debug_uart, USART_FLAG_TC) == RESET)
	{
	}

	return ch;
#endif
}
