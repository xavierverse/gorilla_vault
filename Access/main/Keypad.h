#ifndef __KEYPAD_H__
#define __KEYPAD_H__

#include <stdio.h>
#include <driver/i2c.h>
#include "esp_log.h"

#define I2C_MASTER_FREQ_HZ 100000
#define I2C_MASTER_SDA_IO GPIO_NUM_10
#define I2C_MASTER_SCL_IO GPIO_NUM_8

#define KEYPAD_ADDRESS     0x4B

void setKeypadReg(uint8_t addr, uint8_t data);

uint8_t getButton(void);

#endif