#ifndef P12_PROTOCOL_H
#define P12_PROTOCOL_H

#include "Sci_Upper.h"

#define P12_FRAME_START_0             ((UINT8)0x21)
#define P12_FRAME_START_1             ((UINT8)0xAA)
#define P12_FRAME_MIN_LENGTH          ((UINT8)8)
#define P12_HOST_ADDRESS              ((UINT16)0xFF01)
#define P12_BMS_ADDRESS_DEFAULT       ((UINT16)0x0301)

#define P12_CMD_REQ_STATIC            ((UINT8)0x80)
#define P12_CMD_RSP_STATIC            ((UINT8)0x81)
#define P12_CMD_REQ_DYNAMIC           ((UINT8)0x82)
#define P12_CMD_RSP_DYNAMIC           ((UINT8)0x83)

UINT8 P12_IsStartByte(UINT8 firstByte);
UINT8 P12_CheckHeader(const struct RS485MSG *s);
UINT8 P12_IsRxComplete(const struct RS485MSG *s);
UINT8 P12_PrepareResponse(struct RS485MSG *s);

#endif /* P12_PROTOCOL_H */
