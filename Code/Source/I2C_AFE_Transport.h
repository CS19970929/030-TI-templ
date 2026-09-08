#ifndef I2C_AFE_TRANSPORT_H
#define I2C_AFE_TRANSPORT_H

#include <stdint.h>

/* 公共接口保持0成功、1失败，具体原因单独记录。 */
typedef enum {
    AFE_I2C_OK = 0,
    AFE_I2C_ARGUMENT,
    AFE_I2C_CONTEXT,
    AFE_I2C_CLOCK,
    AFE_I2C_TIMEOUT,
    AFE_I2C_NACK,
    AFE_I2C_BUS_ERROR,
    AFE_I2C_ARBITRATION,
    AFE_I2C_OVERRUN,
    AFE_I2C_UNEXPECTED_STOP,
    AFE_I2C_SCL_STUCK,
    AFE_I2C_SDA_STUCK,
    AFE_I2C_CRC
} AFE_I2C_ERROR;

typedef enum {
    AFE_I2C_STAGE_INIT = 0,
    AFE_I2C_STAGE_IDLE,
    AFE_I2C_STAGE_TX,
    AFE_I2C_STAGE_RX,
    AFE_I2C_STAGE_STOP,
    AFE_I2C_STAGE_CRC
} AFE_I2C_STAGE;

typedef struct {
    AFE_I2C_ERROR lastError;       /* 成功后仍保留最近一次失败记录。 */
    AFE_I2C_STAGE lastStage;
    AFE_I2C_ERROR recoveryError;
    uint32_t failureCount;
    uint32_t recoveryCount;
} AFE_I2C_DIAGNOSTICS;

/* 仅主循环调用，需先执行InitDelay/InitTimer；PB10/PB11为单主机总线。 */
int AFE_I2C_Init(void);
void AFE_I2C_Tick1ms(void);
const AFE_I2C_DIAGNOSTICS *AFE_I2C_GetDiagnostics(void);
int AFE_I2C_ProtocolError(AFE_I2C_ERROR error);
int I2CSendBytes(unsigned char address, unsigned char *buffer,
                 unsigned int length, unsigned int *transferred);
int I2CReadBytes(unsigned char address, unsigned char *buffer,
                 unsigned int length, unsigned int *transferred);

#endif
