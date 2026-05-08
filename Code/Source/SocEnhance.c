#include "SocEnhance.h"

struct SOC_ENHANCE_ELEMENT SOC_Enhance_Element;

UINT16 ChgValue = 0;
UINT16 DsgValue = 0;

const UINT16 SOC_Table_LiFePO[SOC_Size_LiFePO] = {
    4200, 100,
    4000, 100,
    3650, 99,
    3600, 98,
    3500, 96,
    3450, 93,
    3400, 90,
    3360, 85,
    3340, 80,
    3320, 70,
    3300, 60,
    3280, 50,
    3260, 40,
    3240, 30,
    3220, 20,
    3200, 10,
    3150, 4,
    3100, 2,
    3000, 0,
    2800, 0,
    2500, 0,
};

const UINT16 SocTable_TernaryLi[SOC_Size_TernaryLi] = {
    4200, 100,
    4126, 100,
    4066, 95,
    3980, 90,
    3920, 85,
    3874, 80,
    3830, 75,
    3790, 70,
    3750, 65,
    3710, 60,
    3670, 55,
    3630, 50,
    3590, 45,
    3550, 40,
    3510, 35,
    3470, 30,
    3420, 25,
    3360, 20,
    3300, 15,
    3200, 5,
    3000, 0,
};

const UINT16 SocTable_LiFePO2[SOC_Size_LiFePO2] = {
    4200, 100,
    3650, 100,
    3600, 98,
    3550, 97,
    3500, 96,
    3450, 94,
    3400, 90,
    3360, 85,
    3340, 80,
    3320, 70,
    3300, 60,
    3280, 50,
    3260, 40,
    3240, 30,
    3220, 20,
    3200, 10,
    3150, 5,
    3100, 2,
    3000, 0,
    2800, 0,
    2650, 0,
};

UINT16 GetEndValuee(const UINT16 *ptbl, UINT16 tblsize, UINT16 dat)
{
    UINT16 pairs;
    UINT16 i;
    INT32 x1;
    INT32 x2;
    INT32 y1;
    INT32 y2;
    INT32 result;
    UINT8 descending;

    if (ptbl == 0 || tblsize < 4 || (tblsize & 0x01))
    {
        return 0;
    }

    pairs = (UINT16)(tblsize / 2u);
    descending = (ptbl[0] >= ptbl[2]) ? 1u : 0u;

    if (descending)
    {
        if (dat >= ptbl[0])
        {
            return ptbl[1];
        }
        if (dat <= ptbl[(UINT16)((pairs - 1u) * 2u)])
        {
            return ptbl[(UINT16)((pairs - 1u) * 2u + 1u)];
        }
    }
    else
    {
        if (dat <= ptbl[0])
        {
            return ptbl[1];
        }
        if (dat >= ptbl[(UINT16)((pairs - 1u) * 2u)])
        {
            return ptbl[(UINT16)((pairs - 1u) * 2u + 1u)];
        }
    }

    for (i = 0; i < (UINT16)(pairs - 1u); ++i)
    {
        x1 = ptbl[(UINT16)(i * 2u)];
        y1 = ptbl[(UINT16)(i * 2u + 1u)];
        x2 = ptbl[(UINT16)((i + 1u) * 2u)];
        y2 = ptbl[(UINT16)((i + 1u) * 2u + 1u)];
        if ((descending && dat <= x1 && dat >= x2) ||
            (!descending && dat >= x1 && dat <= x2))
        {
            if (x2 == x1)
            {
                return (UINT16)y1;
            }
            result = y1 + (((INT32)dat - x1) * (y2 - y1)) / (x2 - x1);
            if (result < 0)
            {
                result = 0;
            }
            if (result > 100)
            {
                result = 100;
            }
            return (UINT16)result;
        }
    }

    return ptbl[1];
}

UINT16 InverterChgCurve(void)
{
    return SOC_Enhance_Element.u16_Ichg;
}

UINT16 InverterDsgCurve(void)
{
    return SOC_Enhance_Element.u16_Idsg;
}

void SOC_IntEnhance_Ctrl(UINT8 TimeBase_200ms)
{
    (void)TimeBase_200ms;
}
