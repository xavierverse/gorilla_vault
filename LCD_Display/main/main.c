#include <stdio.h>
#include <esp_log.h>
#include <driver/i2c.h>

#include "LCD.h"

void app_main(void) {
    printf("Hello World!\n");

    i2c_init();
    setRGB(0, 255, 255);
    /*while (1) {
        setCursor(0,0);
        printstr("Hello CSE123!");
        setCursor(0,1);
        printstr("Embedded Team!");
    }*/

    while (1) {
        //display();
        setCursor(0,0);
        setRGB(0,255,255);
        printstr("Door 2 Code: ");
        setCursor(6,1);
        //printstr("1");
        write(97);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        //printstr("2");
        write(98);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        //printstr("3");
        write(99);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        write(100);
        //printstr("4");
        vTaskDelay(1000 / portTICK_PERIOD_MS);    
        
        clear();
        setRGB(255,0,0);
        setCursor(0,0);
        printstr("Access Denied!");
        setCursor(0,1);
        printstr("Retry Code!");

        vTaskDelay(8000 / portTICK_PERIOD_MS);
        clear();
    }
}
