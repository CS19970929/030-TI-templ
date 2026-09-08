#include "main.h"

#ifdef I2C_SYSTEM

/* 沿用板级时序值；I2C2时钟来自PCLK，不能套用I2C1的HSI时钟。 */
#define AFE_I2C_TIMING_8M       0x00901D2Bu
#define AFE_I2C_TIMING_48M      0x30E3363Du
#define AFE_I2C_IDLE_MS        10u
#define AFE_I2C_TRANSFER_MS    25u /* 整笔事务预算，包括全部字节。 */
#define AFE_I2C_RELEASE_MS     2u
#define AFE_I2C_POLL_US        10u
#define AFE_I2C_SCL            GPIO_Pin_10
#define AFE_I2C_SDA            GPIO_Pin_11
#define AFE_I2C_PINS           (AFE_I2C_SCL | AFE_I2C_SDA)
#define AFE_I2C_CLEAR_FLAGS    (I2C_ICR_STOPCF | I2C_ICR_NACKCF | I2C_ICR_BERRCF | I2C_ICR_ARLOCF | I2C_ICR_OVRCF)

typedef struct {
    uint32_t startMs;
    uint32_t pollsLeft;
    uint32_t timeoutMs;
} AFE_I2C_DEADLINE;

static volatile uint32_t afeI2cMs;
static uint32_t afeI2cTiming;
static AFE_I2C_DIAGNOSTICS afeI2cDiag;

void AFE_I2C_Tick1ms(void)
{
    ++afeI2cMs;
}

const AFE_I2C_DIAGNOSTICS *AFE_I2C_GetDiagnostics(void)
{
    return &afeI2cDiag;
}

static int AfeI2c_Fail(AFE_I2C_ERROR error, AFE_I2C_STAGE stage)
{
    afeI2cDiag.lastError = error;
    afeI2cDiag.lastStage = stage;
    ++afeI2cDiag.failureCount;
    /* 旧错误标志为8位，仅锁存一次，避免连续错误计数回绕。 */
    if (!System_ErrFlag.u8ErrFlag_Com_AFE1)
        System_ERROR_UserCallback(ERROR_AFE1);
    return 1;
}

int AFE_I2C_ProtocolError(AFE_I2C_ERROR error)
{
    return AfeI2c_Fail(error, AFE_I2C_STAGE_CRC);
}

static AFE_I2C_DEADLINE AfeI2c_Deadline(uint32_t ms)
{
    AFE_I2C_DEADLINE deadline;
    deadline.startMs = afeI2cMs;
    deadline.timeoutMs = ms;
    deadline.pollsLeft = ms * (1000u / AFE_I2C_POLL_US);
    return deadline;
}

static int AfeI2c_Expired(AFE_I2C_DEADLINE *deadline)
{
    if (((uint32_t)(afeI2cMs - deadline->startMs) >= deadline->timeoutMs) ||
        (deadline->pollsLeft == 0))
        return 1;
    --deadline->pollsLeft;
    /* 1ms节拍停止时以校准延时兜底，不使用未经校准的CPU空转次数。 */
    __delay_us(AFE_I2C_POLL_US);
    return 0;
}

static AFE_I2C_ERROR AfeI2c_StatusError(uint32_t status)
{
    if (status & I2C_ISR_ARLO) return AFE_I2C_ARBITRATION;
    if (status & I2C_ISR_BERR) return AFE_I2C_BUS_ERROR;
    if (status & I2C_ISR_OVR) return AFE_I2C_OVERRUN;
    if (status & I2C_ISR_NACKF) return AFE_I2C_NACK;
    return AFE_I2C_OK;
}

static AFE_I2C_ERROR AfeI2c_Wait(uint32_t flag, AFE_I2C_DEADLINE *deadline)
{
    for (;;)
    {
        uint32_t status = I2C_AFE->ISR;
        AFE_I2C_ERROR error = AfeI2c_StatusError(status);
        /* 错误优先于STOPF，尤其不能将末字节NACK误判为成功。 */
        if (error != AFE_I2C_OK) return error;
        if (status & flag) return AFE_I2C_OK;
        if (status & I2C_ISR_STOPF) return AFE_I2C_UNEXPECTED_STOP;
        if (AfeI2c_Expired(deadline)) return AFE_I2C_TIMEOUT;
    }
}

static int AfeI2c_WaitIdle(uint32_t timeoutMs)
{
    AFE_I2C_DEADLINE deadline = AfeI2c_Deadline(timeoutMs);
    while (I2C_AFE->ISR & I2C_ISR_BUSY)
        if (AfeI2c_Expired(&deadline)) return 0;
    return 1;
}

static int AfeI2c_WaitScl(void)
{
    AFE_I2C_DEADLINE deadline = AfeI2c_Deadline(AFE_I2C_RELEASE_MS);
    while (!GPIO_ReadInputDataBit(GPIOB, AFE_I2C_SCL))
        if (AfeI2c_Expired(&deadline)) return 0;
    return 1;
}

static void AfeI2c_Pins(GPIOMode_TypeDef mode)
{
    GPIO_InitTypeDef gpio;
    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = AFE_I2C_PINS;
    gpio.GPIO_Mode = mode;
    gpio.GPIO_OType = GPIO_OType_OD;
    gpio.GPIO_PuPd = GPIO_PuPd_NOPULL; /* 板上必须提供外部上拉。 */
    gpio.GPIO_Speed = GPIO_Speed_Level_1;
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource10, GPIO_AF_1);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource11, GPIO_AF_1);
    GPIO_Init(GPIOB, &gpio);
}

static void AfeI2c_Configure(void)
{
    I2C_InitTypeDef config;
    I2C_DeInit(I2C_AFE);
    I2C_StructInit(&config);
    config.I2C_Mode = I2C_Mode_I2C;
    config.I2C_AnalogFilter = I2C_AnalogFilter_Enable;
    config.I2C_DigitalFilter = 0;
    config.I2C_OwnAddress1 = 0;
    config.I2C_Ack = I2C_Ack_Enable;
    config.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    config.I2C_Timing = afeI2cTiming;
    AfeI2c_Pins(GPIO_Mode_AF);
    I2C_Init(I2C_AFE, &config); /* 官方库内部关闭PE、配置并重新开启PE。 */
    /* 本总线仅作为主机，不应以从机身份应答地址0。 */
    I2C_AFE->OAR1 &= ~I2C_OAR1_OA1EN;
}

static AFE_I2C_ERROR AfeI2c_ClearBus(void)
{
    unsigned int pulse;
    /* 接管引脚前关闭外设；开漏输出高表示释放线路。 */
    I2C_Cmd(I2C_AFE, DISABLE);
    GPIO_SetBits(GPIOB, AFE_I2C_PINS);
    AfeI2c_Pins(GPIO_Mode_OUT);
    if (!AfeI2c_WaitScl()) return AFE_I2C_SCL_STUCK;
    for (pulse = 0; pulse < 9 && !GPIO_ReadInputDataBit(GPIOB, AFE_I2C_SDA); ++pulse)
    {
        GPIO_ResetBits(GPIOB, AFE_I2C_SCL);
        __delay_us(5);
        GPIO_SetBits(GPIOB, AFE_I2C_SCL);
        if (!AfeI2c_WaitScl()) return AFE_I2C_SCL_STUCK;
        __delay_us(5);
    }
    /* 先拉低SCL再拉低SDA，以免构造STOP时产生多余START。 */
    GPIO_ResetBits(GPIOB, AFE_I2C_SCL);
    GPIO_ResetBits(GPIOB, AFE_I2C_SDA);
    __delay_us(5);
    GPIO_SetBits(GPIOB, AFE_I2C_SCL);
    if (!AfeI2c_WaitScl()) return AFE_I2C_SCL_STUCK;
    __delay_us(5);
    GPIO_SetBits(GPIOB, AFE_I2C_SDA);
    __delay_us(5);
    return GPIO_ReadInputDataBit(GPIOB, AFE_I2C_SDA) ? AFE_I2C_OK : AFE_I2C_SDA_STUCK;
}

static AFE_I2C_ERROR AfeI2c_Recover(AFE_I2C_ERROR cause)
{
    AFE_I2C_ERROR error = AFE_I2C_OK;
    ++afeI2cDiag.recoveryCount;
    /* 仲裁丢失后禁止强制STOP或GPIO时钟。 */
    if (cause == AFE_I2C_ARBITRATION)
    {
        if (!AfeI2c_WaitIdle(AFE_I2C_IDLE_MS))
            error = AFE_I2C_ARBITRATION;
    }
    else
    {
        if (I2C_AFE->ISR & I2C_ISR_BUSY)
        {
            I2C_GenerateSTOP(I2C_AFE, ENABLE);
            (void)AfeI2c_WaitIdle(AFE_I2C_RELEASE_MS);
        }
        I2C_Cmd(I2C_AFE, DISABLE);
        if ((GPIO_ReadInputData(GPIOB) & AFE_I2C_PINS) != AFE_I2C_PINS)
            error = AfeI2c_ClearBus();
    }
    /* 线路卡死也恢复复用模式，不能将临时GPIO状态留给下一次调用。 */
    GPIO_SetBits(GPIOB, AFE_I2C_PINS);
    AfeI2c_Configure();
    if ((error == AFE_I2C_OK) && !AfeI2c_WaitIdle(AFE_I2C_RELEASE_MS))
        error = AFE_I2C_TIMEOUT;
    afeI2cDiag.recoveryError = error;
    return error;
}

int AFE_I2C_Init(void)
{
    RCC_ClocksTypeDef clocks;
    if (__get_IPSR() || __get_PRIMASK())
        return AfeI2c_Fail(AFE_I2C_CONTEXT, AFE_I2C_STAGE_INIT);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2, ENABLE);
    RCC_GetClocksFreq(&clocks);
    if (clocks.PCLK_Frequency == 8000000u) afeI2cTiming = AFE_I2C_TIMING_8M;
    else if (clocks.PCLK_Frequency == 48000000u) afeI2cTiming = AFE_I2C_TIMING_48M;
    else
    {
        afeI2cTiming = 0;
        I2C_Cmd(I2C_AFE, DISABLE);
        return AfeI2c_Fail(AFE_I2C_CLOCK, AFE_I2C_STAGE_INIT);
    }
    AfeI2c_Configure();
    return 0;
}

static int AfeI2c_Transfer(unsigned char address, unsigned char *buffer,
                          unsigned int length, unsigned int *transferred, int read)
{
    unsigned int index;
    AFE_I2C_ERROR error;
    AFE_I2C_STAGE stage = AFE_I2C_STAGE_IDLE;
    AFE_I2C_DEADLINE deadline;
    if (transferred) *transferred = 0;
    if (!buffer || !transferred || !length || length > 255u || address > 0x7Fu)
        return AfeI2c_Fail(AFE_I2C_ARGUMENT, stage);
    if (__get_IPSR() || __get_PRIMASK())
        return AfeI2c_Fail(AFE_I2C_CONTEXT, stage);
    if (!afeI2cTiming) return AfeI2c_Fail(AFE_I2C_CLOCK, stage);
    /* 仅START前允许恢复后继续，START发出后失败不能自动重放写入。 */
    if (!AfeI2c_WaitIdle(AFE_I2C_IDLE_MS) ||
        ((GPIO_ReadInputData(GPIOB) & AFE_I2C_PINS) != AFE_I2C_PINS))
    {
        error = AfeI2c_Recover(AfeI2c_StatusError(I2C_AFE->ISR));
        if (error != AFE_I2C_OK) return AfeI2c_Fail(error, stage);
    }
    I2C_ClearFlag(I2C_AFE, AFE_I2C_CLEAR_FLAGS);
    deadline = AfeI2c_Deadline(AFE_I2C_TRANSFER_MS);
    /* SADD仅为左移后的7位地址，RD_WRN由标准库的读写启动参数决定。 */
    I2C_TransferHandling(I2C_AFE, (uint16_t)address << 1, (uint8_t)length,
                        I2C_AutoEnd_Mode, read ? I2C_Generate_Start_Read : I2C_Generate_Start_Write);
    stage = read ? AFE_I2C_STAGE_RX : AFE_I2C_STAGE_TX;
    for (index = 0; index < length; ++index)
    {
        error = AfeI2c_Wait(read ? I2C_ISR_RXNE : I2C_ISR_TXIS, &deadline);
        if (error != AFE_I2C_OK) goto failed;
        if (read) buffer[index] = I2C_ReceiveData(I2C_AFE);
        else I2C_SendData(I2C_AFE, buffer[index]);
        ++*transferred; /* 外设搬运字节数，不表示从机已全部ACK。 */
    }
    stage = AFE_I2C_STAGE_STOP;
    error = AfeI2c_Wait(I2C_ISR_STOPF, &deadline);
    if (error != AFE_I2C_OK) goto failed;
    I2C_ClearFlag(I2C_AFE, I2C_ICR_STOPCF);
    if (!AfeI2c_WaitIdle(AFE_I2C_RELEASE_MS))
    {
        error = AFE_I2C_TIMEOUT;
        goto failed;
    }
    return 0;

failed:
    (void)AfeI2c_Recover(error);
    return AfeI2c_Fail(error, stage);
}

int I2CSendBytes(unsigned char address, unsigned char *buffer,
                 unsigned int length, unsigned int *transferred)
{
    return AfeI2c_Transfer(address, buffer, length, transferred, 0);
}

int I2CReadBytes(unsigned char address, unsigned char *buffer,
                 unsigned int length, unsigned int *transferred)
{
    return AfeI2c_Transfer(address, buffer, length, transferred, 1);
}

#endif
