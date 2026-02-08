#include <stdio.h>
#include <driver/i2c.h>
#include "esp_log.h"

#define I2C_MASTER_FREQ_HZ 100000
#define I2C_MASTER_SDA_IO GPIO_NUM_10
#define I2C_MASTER_SCL_IO GPIO_NUM_8

//#define KEYPAD_ADDRESS     (0x4B>>1)
#define KEYPAD_ADDRESS     0x4B

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
}

void setReg(uint8_t addr, uint8_t data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (KEYPAD_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, addr, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
}


void app_main(void) {
    printf("Hello World!\n");
    i2c_init(); 

    while (1){
        //update fifo
        setReg(0x06, 0x01);

        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (KEYPAD_ADDRESS << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd, 0x03, true);
        i2c_master_stop(cmd);
        i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
        i2c_cmd_link_delete(cmd);

        //uint8_t buffer_char[1024];
        uint8_t data = 0;

        cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (KEYPAD_ADDRESS << 1) | I2C_MASTER_READ, true);
        i2c_master_read_byte(cmd, &data, true);
        i2c_master_stop(cmd);
        i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
        i2c_cmd_link_delete(cmd);

        ESP_LOGI("TAG", "Character: %c \t%d", data, data);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

}
