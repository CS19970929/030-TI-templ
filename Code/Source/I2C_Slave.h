#ifndef I2C_SLAVE_H
#define I2C_SLAVE_H


#define SLAVE_ADDR          0x0A
#define I2CSPEED            100000   //i2cËÙ¶È

#define I2C_BUFF_D0_SIZE    40
#define I2C_BUFF_D1_SIZE    40

#define I2C_CMD_D0          0xD0
#define I2C_CMD_D1          0xD1

void Init_I2CSlaver(void);
void I2C_Salve_Deal(void);

#endif	/* I2C_SLAVE_H */

