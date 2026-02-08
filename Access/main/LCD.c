#include <driver/i2c.h>
#include <inttypes.h>
#include <string.h>

#include "LCD.h"

uint8_t _showfunction;
uint8_t _showcontrol;
uint8_t _showmode;
uint8_t _cols;
uint8_t _rows;


void i2c_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,         // select GPIO specific to your project
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,         // select GPIO specific to your project
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,  // select frequency specific to your project
    };

    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));

    _showfunction = LCD_4BITMODE | LCD_1LINE | LCD_5x8DOTS;
    begin(16, 2, LCD_5x8DOTS);
}

void begin(uint8_t cols, uint8_t rows, uint8_t charsize) {
    if (rows > 1) {
        _showfunction |= LCD_2LINE;
    }

    if ((charsize != 0) && (rows == 1)) {
        _showfunction |= LCD_5x10DOTS;
    }

    // 40 ms delay
    vTaskDelay(50 / portTICK_PERIOD_MS);

    // Send function set command sequence
    command(LCD_FUNCTIONSET | _showfunction);
    vTaskDelay(5 / portTICK_PERIOD_MS);

    // second try
    command(LCD_FUNCTIONSET | _showfunction);
    vTaskDelay(5 / portTICK_PERIOD_MS);
    
    // third go
    command(LCD_FUNCTIONSET | _showfunction);

    // turn the display on with no cursor or blinking default
    _showcontrol = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
    display();

    // clear it off
    clear();

    // set the entry mode
    _showmode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
    command(_showmode);

    // backlight init
    setReg(REG_MODE1, 0x00);   

    // set LEDs controllable by both PWM and GRPPWM registers
    setReg(REG_OUTPUT, 0xFF); 
    
    // set MODE2 values
    setReg(REG_MODE2, 0x20);

    // set rgb to white
    setColorWhite();
}

void clear(void) {
    command(LCD_CLEARDISPLAY);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
}

void display(void) {
    _showcontrol |= LCD_DISPLAYON;
    command(LCD_DISPLAYCONTROL | _showcontrol);
}

void setCursor(uint8_t col, uint8_t row)  {
    col = (row == 0 ? col|0x80 : col|0xc0);
    uint8_t data[3] = {0x80, col};
    LCDsend(data, 2);
}

void setColorWhite(void) {
    setRGB(255, 255, 255);
}

void printstr(const char c[]) {
    size_t arrSize = strlen(c);
	for (int i=0; i<arrSize; i++) {
		LCDwrite(c[i]);
	}
}

void command(uint8_t value) {
    uint8_t data[3] = {0x80, value};
    LCDsend(data, 2);
}

void LCDsend(uint8_t *data, uint8_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (LCD_ADDRESS << 1) | I2C_MASTER_WRITE, true);

    for (int i=0; i<len; i++) {
        i2c_master_write_byte(cmd, data[i], true);
    }

    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
}

void setRGB(uint8_t r, uint8_t g, uint8_t b) {
    setReg(REG_RED, r);
    setReg(REG_GREEN, g);
    setReg(REG_BLUE, b);
}

void setReg(uint8_t addr, uint8_t data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (RGB_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, addr, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
}

void LCDwrite(uint8_t value) {
    uint8_t data[3] = {0x40, value};
    LCDsend(data, 2);
}
