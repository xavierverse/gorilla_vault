#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"

// Getting Top Door Communication
#define TopDoor_GPIO GPIO_NUM_1

static uint8_t signal_value = 0;

// Getting Bot Door Communication
#define BotDoor_GPIO GPIO_NUM_6

static uint8_t signal_bot_value = 0;

// Setting Mid Door Communication
#define MidDoor_GPIO GPIO_NUM_7

static const char *TAG = "Recieving ESP";

static void configure_DoorCom (void) {
    //Top Door 
    gpio_reset_pin(TopDoor_GPIO);
    ESP_LOGI(TAG, "Configure Top Door Communication");
    gpio_set_direction(TopDoor_GPIO, GPIO_MODE_INPUT);

    //Bot Door 
    gpio_reset_pin(BotDoor_GPIO);
    ESP_LOGI(TAG, "Configure Bot Door Communication");
    gpio_set_direction(BotDoor_GPIO, GPIO_MODE_INPUT);

    //Mid Door 
    gpio_reset_pin(MidDoor_GPIO);
    ESP_LOGI(TAG, "Configure Middle Door Communication");
    gpio_set_direction(MidDoor_GPIO, GPIO_MODE_OUTPUT);
}

//Top Door Communication
void TopDoorCom(void *pvParameters) {
    while(1) {
        signal_value = gpio_get_level(TopDoor_GPIO);
        ESP_LOGI(TAG, "Recieved Top Status: %d", signal_value);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

//Bot Door Communication
void BotDoorCom(void *pvParameters) {
    while(1) {
        signal_bot_value = gpio_get_level(BotDoor_GPIO);
        ESP_LOGI(TAG, "Recieved Bot Status: %d", signal_bot_value);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

//Mid Door Communication
void MidDoorCom(void *pvParameters) {
    while(1) {
        ESP_LOGI(TAG, "Sending a 0");
        gpio_set_level(MidDoor_GPIO, 0);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        //ESP_LOGI(TAG, "Sending a 0");
        //gpio_set_level(MidDoor_GPIO, 0);
        //vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}


void app_main(void)
{
    printf("Hello World!\n");
    configure_DoorCom();
    xTaskCreate(&TopDoorCom, "Top Door Com", 2048, NULL, 1, NULL);
    xTaskCreate(&BotDoorCom, "Bot Door Com", 2048, NULL, 1, NULL);
    xTaskCreate(&MidDoorCom, "Bot Door Com", 2048, NULL, 1, NULL);
}