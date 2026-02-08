#ifndef __LCD_H__
#define __LCD_H__

#include <inttypes.h>

#define I2C_MASTER_FREQ_HZ 100000
#define I2C_MASTER_SDA_IO GPIO_NUM_10
#define I2C_MASTER_SCL_IO GPIO_NUM_8

// Device I2C Address
#define LCD_ADDRESS     (0x7C>>1)
#define RGB_ADDRESS     (0xC0>>1)

#define REG_RED         0x04        // pwm2
#define REG_GREEN       0x03        // pwm1
#define REG_BLUE        0x02        // pwm0

#define REG_MODE1       0x00
#define REG_MODE2       0x01
#define REG_OUTPUT      0x08

// Commands
#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT 0x10
#define LCD_FUNCTIONSET 0x20
#define LCD_SETCGRAMADDR 0x40
#define LCD_SETDDRAMADDR 0x80

// Flags for Display Entry Mode
#define LCD_ENTRYRIGHT 0x00
#define LCD_ENTRYLEFT 0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

// Flags for Display on/off Control
#define LCD_DISPLAYON 0x04
#define LCD_DISPLAYOFF 0x00
#define LCD_CURSORON 0x02
#define LCD_CURSOROFF 0x00
#define LCD_BLINKON 0x01
#define LCD_BLINKOFF 0x00

// Flags for display/cursor shift
#define LCD_DISPLAYMOVE 0x08
#define LCD_CURSORMOVE 0x00
#define LCD_MOVERIGHT 0x04
#define LCD_MOVELEFT 0x00

// Flags for function set
#define LCD_8BITMODE 0x10
#define LCD_4BITMODE 0x00
#define LCD_2LINE 0x08
#define LCD_1LINE 0x00
#define LCD_5x10DOTS 0x04
#define LCD_5x8DOTS 0x00

// initialize I2C
void i2c_init(void);

// initialize LCD
void begin(uint8_t cols, uint8_t rows, uint8_t charsize);

// clear the display
void clear(void);

// turn the display on
void display(void);

// Move Cursor
void setCursor(uint8_t col, uint8_t row); 

// set the RGB 
void setRGB(uint8_t r, uint8_t g, uint8_t b);

// set to White
void setColorWhite(void);

// send command
void command(uint8_t value);

// print a string
void printstr(const char c[]);

// send i2c command to lcd 
void send(uint8_t *data, uint8_t len);

// send i2c command to RGB
void setReg(uint8_t addr, uint8_t data);

// send data
void write(uint8_t value);

#endif