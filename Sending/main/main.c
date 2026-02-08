#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"

// Sending Top Door Communication
#define TopDoor_GPIO GPIO_NUM_7
#define BotDoor_GPIO GPIO_NUM_8

static const char *TAG = "Sending ESP";

static uint8_t One = 1;
static uint8_t Zero = 0;

//Door Configurations
static void configure_DoorCom (void) {
    //Top Door 
    gpio_reset_pin(TopDoor_GPIO);
    ESP_LOGI(TAG, "Configure Top Door Communication");
    gpio_set_direction(TopDoor_GPIO, GPIO_MODE_OUTPUT);
    //Bot Door 
    gpio_reset_pin(BotDoor_GPIO);
    ESP_LOGI(TAG, "Configure Bot Door Communication");
    gpio_set_direction(BotDoor_GPIO, GPIO_MODE_OUTPUT);
}

//Top Door Communication
void TopDoorCom(void *pvParameters) {
    while(1) {
        //Top Door Communication
        ESP_LOGI(TAG, "Sending Top Door: %d", 1);
        ESP_ERROR_CHECK(gpio_set_level(TopDoor_GPIO, 1&&1));
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, "Sending Top Door: %d", 0);
        ESP_ERROR_CHECK(gpio_set_level(TopDoor_GPIO, 1&&0));
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

void BotDoorCom(void *pvParameters) {
    while(1) {
        //Bot Door Communication
        ESP_LOGI(TAG, "Sending Bot Door: %d", 0);
        ESP_ERROR_CHECK(gpio_set_level(BotDoor_GPIO, 1&&0));
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, "Sending Bot Door: %d", 1);
        ESP_ERROR_CHECK(gpio_set_level(BotDoor_GPIO, 1&&1));
        vTaskDelay(5000 / portTICK_PERIOD_MS);

    }
}


void app_main(void) {
    printf("Hello World from ESP32!\n");
    configure_DoorCom();
    xTaskCreate(&TopDoorCom, "Top Door Com", 2048, NULL, 1, NULL);
    xTaskCreate(&BotDoorCom, "Top Door Com", 2048, NULL, 1, NULL);
}