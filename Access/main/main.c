#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "driver/adc.h"

#include <sys/param.h>
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"

#include "LCD.h"
#include "Station.h"
#include "Keypad.h"
#include "Barcode.h"


static const char *TAG = "Embedded Team";

#define BLINK_PERIOD 100
#define BUTTON_PERIOD 100

// Setting LED 1 Configuration
#define LED1_GPIO GPIO_NUM_2
static uint8_t s_led1_state = 1;
    
// Setting Button 1 Configuration
#define BTN1_GPIO GPIO_NUM_3
static uint8_t button1 = 0;
static uint8_t BTN1_STATE = 1;

// Setting LED 2 Configuration
#define LED2_GPIO GPIO_NUM_4
static uint8_t s_led2_state = 0;

// Setting Button 2 Configuration
#define BTN2_GPIO GPIO_NUM_5
static uint8_t button2 = 0;
static uint8_t BTN2_STATE = 0;

//Motion Sensor Input (subject to change for the esp32cam)
#define MOTION ADC_CHANNEL_0
static uint8_t motion_detected = 0;

static uint8_t motionState = 0;

// Sending Top Door Communication
#define TopDoor_GPIO GPIO_NUM_1

// Sending Bottom Door Communication
#define BotDoor_GPIO GPIO_NUM_6

// Recieving Middle Door Communication
#define MiddleDoor_GPIO GPIO_NUM_7
// 0 = closed, 1 = open
static uint8_t middleDoorStatus = 0;

// LCD Display
//screen 1 = Code Entry, screen 2 = Denied/Accepted, screen 3 = Waiting for Middle Door to close
static uint8_t screen = 1;
static uint8_t col = 6;

// Keypad
static uint8_t keyData = 0;
static uint8_t codeCounter = 0;

// Codes: 7789, 1725, 1108, 0022
static uint8_t quotationCount = 0;
static uint8_t codeIndex = 0;
static uint8_t closedBracket = 0;
static uint8_t correctCode[256][4] = {0};
uint8_t userCode[4] = {0};
static uint8_t keyCode[4] = {0,0,0,0};
static uint8_t counter = 0;
static uint8_t validBEntry = 0;

// Universal Access: 0 = denied, 1 = accepted
static uint8_t uniAccess = 0;

// API Key
//#define API_KEY "Bearer VmF1bHQ0NDM0"
#define API_KEY "Bearer Vk4zNzEwMw=="



extern const char howsmyssl_com_root_cert_pem_start[] asm("_binary_howsmyssl_com_root_cert_pem_start");
extern const char howsmyssl_com_root_cert_pem_end[]   asm("_binary_howsmyssl_com_root_cert_pem_end");

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 128

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    static char *output_buffer;  // Buffer to store response of http request from event handler
    static int output_len;       // Stores number of bytes read
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            // Clean the buffer in case of a new request
            if (output_len == 0 && evt->user_data) {
                // we are just starting to copy the output data into the use
                memset(evt->user_data, 0, MAX_HTTP_OUTPUT_BUFFER);
            }
            /*
             *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
             *  However, event handler can also be used in case chunked encoding is used.
             */
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // If user_data buffer is configured, copy the response into the buffer
                int copy_len = 0;
                if (evt->user_data) {
                    // The last byte in evt->user_data is kept for the NULL character in case of out-of-bound access.
                    copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
                    if (copy_len) {
                        memcpy(evt->user_data + output_len, evt->data, copy_len);
                    }
                } else {
                    int content_len = esp_http_client_get_content_length(evt->client);
                    if (output_buffer == NULL) {
                        // We initialize output_buffer with 0 because it is used by strlen() and similar functions therefore should be null terminated.
                        output_buffer = (char *) calloc(content_len + 1, sizeof(char));
                        output_len = 0;
                        if (output_buffer == NULL) {
                            ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                            return ESP_FAIL;
                        }
                    }
                    copy_len = MIN(evt->data_len, (content_len - output_len));
                    if (copy_len) {
                        memcpy(output_buffer + output_len, evt->data, copy_len);
                    }
                }
                output_len += copy_len;
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            if (output_buffer != NULL) {
                // Response is accumulated in output_buffer. Uncomment the below line to print the accumulated response
                // ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            esp_http_client_set_header(evt->client, "From", "user@example.com");
            esp_http_client_set_header(evt->client, "Accept", "text/html");
            esp_http_client_set_redirection(evt->client);
            break;
    }
    return ESP_OK;
}

int asciiToDecimal(uint8_t asciiDigit) {
    return asciiDigit - '0';
}

static void blink_led(uint8_t BLINK_GPIO, uint8_t s_led_state) {
    /* Set the GPIO level according to the state (LOW or HIGH)*/
    gpio_set_level(BLINK_GPIO, s_led_state);
}

static void configure_led(void){
    ESP_LOGI(TAG, "Blink GPIO LED!");
    gpio_reset_pin(LED1_GPIO);
    gpio_reset_pin(LED2_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(LED1_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED2_GPIO, GPIO_MODE_OUTPUT);
}

void LED1(void *pvParameters) {
    while (1) {
        //ESP_LOGI(TAG, "Turning the LED1 %s!", s_led1_state == true ? "ON" : "OFF");
        blink_led(LED1_GPIO, s_led1_state);
        vTaskDelay(BLINK_PERIOD / portTICK_PERIOD_MS);
    }
}

void LED2(void *pvParameters) {
    while(1) {
        //ESP_LOGI(TAG, "Turning the LED2 %s", s_led2_state == true ? "ON" : "OFF");
        blink_led(LED2_GPIO, s_led2_state);
        vTaskDelay(BLINK_PERIOD / portTICK_PERIOD_MS);
    }
}

static void configure_button(void){
    //ESP_LOGI(TAG, "Configure Button1 GPIO!");
   // gpio_reset_pin(BTN1_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BTN1_GPIO, GPIO_MODE_INPUT);
    gpio_set_direction(BTN2_GPIO, GPIO_MODE_INPUT);
}

void Button1(void *pvParameters) {
    while(1) {
        //ESP_LOGI(TAG, "Button1 state %s!", BTN1_STATE == true ? "ON" : "OFF");
        button1 = gpio_get_level(BTN1_GPIO);
        
        if (button1 == 1) {
            if (screen == 1) {
                setCursor(0,0);
                printstr("Top");
                setCursor(col,1);
            }
            BTN1_STATE = 1;
            s_led1_state = 1;
            BTN2_STATE = 0;
            s_led2_state = 0;

        }

        vTaskDelay(BUTTON_PERIOD / portTICK_PERIOD_MS);
    }

}

void Button2(void *pvParameters) {
    while(1) {
        //ESP_LOGI(TAG, "Button2 state %s!", BTN2_STATE == true ? "ON" : "OFF");
        button2 = gpio_get_level(BTN2_GPIO);
         
         if (button2 == 1) {
            if (screen == 1) {
                setCursor(0,0);
                printstr("Bot"); 
                setCursor(col,1);
            }
            BTN1_STATE = 0;
            s_led1_state = 0;
            BTN2_STATE = 1;
            s_led2_state = 1;
        }

        vTaskDelay(BUTTON_PERIOD / portTICK_PERIOD_MS);
    }

}

//motion sensor task
static void configure_motion(void){
    //ESP_LOGI(TAG, "Configure motion sensor GPIO!");
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);
}

void MotionSensor(void *pvParameters) {
    uint8_t send_notty = 1;
    while(1) {
        motion_detected = adc1_get_raw(ADC1_CHANNEL_0);
        //ESP_LOGI(TAG, "MotionSensor Data: %d", motion_detected);
        if ((motion_detected>=230) && (send_notty==1)) {
            //ESP_LOGI(TAG, "\t\t\tSEND NOTIFICATION!");
        //    http_put_task("/set_motion/7789", "{\"motion_status\":\"true\"}");


        // PUT HTTPS
        motionState = 1;

        char motion_buffer[MAX_HTTP_OUTPUT_BUFFER + 1] = {0};
        esp_http_client_config_t config = {
            .host = "www.gorillavault.xyz",
            .port = 443,
            .path = "/motion",
            .transport_type = HTTP_TRANSPORT_OVER_SSL,
            .event_handler = _http_event_handler,
            .user_data = motion_buffer,        // Pass address of local buffer to get response
            .disable_auto_redirect = true,
            .cert_pem = howsmyssl_com_root_cert_pem_start,
        };


        esp_http_client_handle_t client = esp_http_client_init(&config);

        const char *post_data = "{\"motion_status\":\"true\"}";
        esp_http_client_set_url(client, "https://gorillavault.xyz/motion");
        esp_http_client_set_method(client, HTTP_METHOD_PUT);
        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_header(client, "Authorization", API_KEY);
        esp_http_client_set_post_field(client, post_data, strlen(post_data));

        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %"PRId64,
                    esp_http_client_get_status_code(client),
                    esp_http_client_get_content_length(client));
        } else {
            ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
        }
        ESP_LOGI(TAG, " MOTION STATUS TRUE BUFFER: %s", motion_buffer);

        esp_http_client_cleanup(client);



            send_notty = 0;
        }
        if ((motion_detected<230) & motionState) {
            send_notty = 1;
            motionState = 0;

            // PUT HTTPS

        char motion_buffer[MAX_HTTP_OUTPUT_BUFFER + 1] = {0};
        esp_http_client_config_t config = {
            .host = "www.gorillavault.xyz",
            .port = 443,
            .path = "/motion",
            .transport_type = HTTP_TRANSPORT_OVER_SSL,
            .event_handler = _http_event_handler,
            .user_data = motion_buffer,        // Pass address of local buffer to get response
            .disable_auto_redirect = true,
            .cert_pem = howsmyssl_com_root_cert_pem_start,
        };

       
        esp_http_client_handle_t client = esp_http_client_init(&config);

        const char *post_data = "{\"motion_status\":\"false\"}";
        esp_http_client_set_url(client, "https://gorillavault.xyz/motion");
        esp_http_client_set_method(client, HTTP_METHOD_PUT);
        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_header(client, "Authorization", API_KEY);
        esp_http_client_set_post_field(client, post_data, strlen(post_data));

        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %"PRId64,
                    esp_http_client_get_status_code(client),
                    esp_http_client_get_content_length(client));
        } else {
            ;
            ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
        }
        ESP_LOGI(TAG, "MOTION STATUS FALSE BUFFER: %s", motion_buffer);


        esp_http_client_cleanup(client);
        
        }

        
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

//Door Configurations
static void configure_DoorCom (void) {
    //Top Door 
    gpio_reset_pin(TopDoor_GPIO);
    ESP_LOGI(TAG, "Configure Top Door Communication");
    gpio_set_direction(TopDoor_GPIO, GPIO_MODE_OUTPUT);

    //Bottom Door
    gpio_reset_pin(BotDoor_GPIO);
    ESP_LOGI(TAG, "Configure Bottom Door Communication");
    gpio_set_direction(BotDoor_GPIO, GPIO_MODE_OUTPUT);

    //Middle Door
    gpio_reset_pin(MiddleDoor_GPIO);
    ESP_LOGI(TAG, "Configure Middle Door Communication");
    gpio_set_direction(MiddleDoor_GPIO, GPIO_MODE_INPUT);
}

//Top Door Communication
void TopDoorCom(void *pvParameters) {
    while(1) {
        //ESP_LOGI(TAG, "Sending Top Door: %d\n", (BTN1_STATE&&uniAccess&&!middleDoorStatus));
        //ESP_LOGI(TAG, "Sending Top Door: %d\n", (BTN1_STATE&&uniAccess));
        ESP_ERROR_CHECK(gpio_set_level(TopDoor_GPIO, (BTN1_STATE&&uniAccess&&!middleDoorStatus)));
       // ESP_LOGI(TAG, "TOP DOOR SIGNAL: %u", (BTN1_STATE&&uniAccess&&!middleDoorStatus));
        //ESP_ERROR_CHECK(gpio_set_level(TopDoor_GPIO, 0));
       // ESP_LOGI(TAG, "uniAccess: %d", uniAccess );
        //ESP_LOGI(TAG, "BTN1: %d", BTN1_STATE );

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

//Bottom Door Communication
void BotDoorCom(void *pvParameters) {
    while(1) {
       //ESP_LOGI(TAG, "Sending Bot Door: %d\n", (BTN2_STATE&&uniAccess));
        ESP_ERROR_CHECK(gpio_set_level(BotDoor_GPIO, (BTN2_STATE&&uniAccess)));
       // ESP_LOGI(TAG, "BOT DOOR SIGNAL: %u", (BTN2_STATE&&uniAccess));
        //ESP_ERROR_CHECK(gpio_set_level(BotDoor_GPIO, 0));
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

//Middle Door Communication
void MiddleDoorCom(void *pvParameters) {
    while (1) {
        middleDoorStatus = gpio_get_level(MiddleDoor_GPIO);
       // ESP_LOGI(TAG, "Middle Door: %s", middleDoorStatus == true ? "OPEN" : "CLOSED");
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

// LCD Display
void Display(void *pvParameters) {
    while (1) {
        screen = 1;
        //display();
        setCursor(0,0);
        setRGB(0,255,255);
        
        if (BTN1_STATE) {
            printstr("Top Door Code: ");
        } else if (BTN2_STATE) {
            printstr("Bot Door Code: ");
        }

        setCursor(6,1);
        col = 6;

        while(codeCounter != 4){
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

        //if incorrect code:
        if (BTN1_STATE) {
          //  http_get_task("/packages/undelivered");

         //HTTPS
        char packages_buffer[MAX_HTTP_OUTPUT_BUFFER + 1] = {0};
            esp_http_client_config_t config = {
                .host = "www.gorillavault.xyz",
                .port = 443,
                .path = "/undelivered_packages",
                .transport_type = HTTP_TRANSPORT_OVER_SSL,
                .event_handler = _http_event_handler,
                .user_data = packages_buffer,        // Pass address of local buffer to get response
                .disable_auto_redirect = true,
                .cert_pem = howsmyssl_com_root_cert_pem_start,
            };
            esp_http_client_handle_t client = esp_http_client_init(&config);
            esp_http_client_set_header(client, "Authorization", API_KEY);
            esp_err_t err = esp_http_client_perform(client);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %"PRId64,
                        esp_http_client_get_status_code(client),
                        esp_http_client_get_content_length(client));
            } else {
               // ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
            }
            ESP_LOGI(TAG, "LIST OF PACKAGES BUFFER: %s", packages_buffer);
            int b = 0;
            while(!closedBracket){
                //ESP_LOGI(TAG, "in while loop");
                if(packages_buffer[b] == 93){ //checks for closed bracket
                    closedBracket = 1;
                }
                else if(packages_buffer[b] == 34){ //checks for quotation mark
                    quotationCount += 1;
                    if((quotationCount % 2) == 1){ //means if the quotation mark is an odd number (start of package code)
                        for(int j = 0; j < 4; j++){
                            correctCode[codeIndex][j] = packages_buffer[b+(j+1)]; //do j +1 because i dont want quotation mark included
                        }
                        codeIndex += 1;
                    }
                }
                b++;
            }
            esp_http_client_cleanup(client);


            for (int i=0; i<codeIndex; i++) {
                counter = 0;
                for (int j=0; j<4; j++) {
                    if (keyCode[j] != correctCode[i][j]) {
                        uniAccess = 0;
                        break;
                    }
                uniAccess = 1;
                counter++;
                }
                if (counter == 4) {
                    break;
                }
            }
        } else if (BTN2_STATE) {
            //http_get_task("/get_vault_code/7789");

            //HTTPS

            char vault_code_buffer[MAX_HTTP_OUTPUT_BUFFER + 1] = {0};
            esp_http_client_config_t config = {
                .host = "www.gorillavault.xyz",
                .port = 443,
                .path = "/vault_code",
                .transport_type = HTTP_TRANSPORT_OVER_SSL,
                .event_handler = _http_event_handler,
                .user_data = vault_code_buffer,        // Pass address of local buffer to get response
                .disable_auto_redirect = true,
                .cert_pem = howsmyssl_com_root_cert_pem_start,
            };
            esp_http_client_handle_t client = esp_http_client_init(&config);
            esp_http_client_set_header(client, "Authorization", API_KEY);
            esp_err_t err = esp_http_client_perform(client);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %"PRId64,
                        esp_http_client_get_status_code(client),
                        esp_http_client_get_content_length(client));
            } else {
                ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
            }
           // ESP_LOGI(TAG, "VAULT CODE BUFFER: %s", vault_code_buffer);
            esp_http_client_cleanup(client);
            int v = 0;
            while(v < 4){
                userCode[v] = vault_code_buffer[v];
                //ESP_LOGI(TAG, "USER CODE BUFFER: %d", userCode[v]);
                v++;
            }

            for (int i=0; i<4; i++) {
                if (keyCode[i] != userCode[i]) {
                   // ESP_LOGI(TAG, "keyCode[i] = %d", keyCode[i]);
                    //ESP_LOGI(TAG, "correctCode[i] = %d", userCode[i]);
                    uniAccess = 0;
                    break;
                }
                uniAccess = 1;
            }
        }
        quotationCount = 0;
        closedBracket = 0;
        codeIndex = 0;

       vTaskDelay(1000/portTICK_PERIOD_MS);

        screen = 2;
        clear();
        if (!uniAccess) {
            setRGB(255,0,0);
            setCursor(0,0);
            printstr("Access Denied!");
            setCursor(0,1);
            printstr("Retry Code!");
        } else {
            setRGB(0,255,255);
            if (middleDoorStatus && BTN1_STATE) {
                screen = 3;
                setCursor(0,0);
                printstr("Access Granted!");
                setCursor(0,1);
                printstr("Door Unlocking!");
                while (middleDoorStatus) {
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                }
            }
            screen = 2;
            clear();
            setCursor(0,0);
            printstr("Access Granted!");
            setCursor(0,1);
            printstr("Open Door!");
            uniAccess = 0;


           // http_put_task("/set_delivery_status/7789", "{\"package_delivered\":\"true\"}");
            // PUT HTTPS
            char delivered_buffer[MAX_HTTP_OUTPUT_BUFFER + 1] = {0};
            esp_http_client_config_t config = {
                .host = "www.gorillavault.xyz",
                .port = 443,
                .path = "/",
                .transport_type = HTTP_TRANSPORT_OVER_SSL,
                .event_handler = _http_event_handler,
                .user_data = delivered_buffer,        // Pass address of local buffer to get response
                .disable_auto_redirect = true,
                .cert_pem = howsmyssl_com_root_cert_pem_start,
            };

            esp_http_client_handle_t client = esp_http_client_init(&config);

            char deliveredInfo[256] = {0};

           int keyCode3 = asciiToDecimal(keyCode[3]);
           int keyCode2 = asciiToDecimal(keyCode[2]);
           int keyCode1 = asciiToDecimal(keyCode[1]);
           int keyCode0 = asciiToDecimal(keyCode[0]);

            snprintf(deliveredInfo, sizeof(deliveredInfo),"{\"package_id\":\"%d%d%d%d\", \"delivery_status\":\"true\"}", keyCode0, keyCode1, keyCode2, keyCode3);

            ESP_LOGI(TAG, "DELIVERED INFO BUFFER: %s", deliveredInfo);

            const char *put_data = deliveredInfo;
           // ESP_LOGI(TAG, "PUTDATA: %c", put_data);
            esp_http_client_set_url(client, "https://gorillavault.xyz/delivery_status");
            // "package_id" : "PACKAGE ID", "delivery_status": "true"
            esp_http_client_set_method(client, HTTP_METHOD_PUT);
            esp_http_client_set_header(client, "Content-Type", "application/json");
            esp_http_client_set_header(client, "Authorization", API_KEY);
            esp_http_client_set_post_field(client, put_data, strlen(put_data));

            esp_err_t err = esp_http_client_perform(client);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %"PRId64,
                        esp_http_client_get_status_code(client),
                        esp_http_client_get_content_length(client));
            } else {
                ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
            }

           ESP_LOGI(TAG, "DELIVERY STATUS BUFFER: %s", delivered_buffer);

            esp_http_client_cleanup(client);
        }



        vTaskDelay(8000 / portTICK_PERIOD_MS);
        codeCounter = 0;
        //reset access
        uniAccess = 0;
        clear();
    }
}

void Keys(void *pvParameters) {
    while (1) {
        //update fifo
        setKeypadReg(0x06, 0x01);

        keyData = getButton();

        if (keyData != 0 && screen == 1 && (codeCounter < 4)){
           // ESP_LOGI("TAG", "Character: %c \t%d", keyData, keyData);
            LCDwrite(keyData);
            keyCode[codeCounter] = keyData;
            codeCounter++;
            col++;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);

    }
}

// Barcode Recieve Task
static void rx_task(void *arg)
{
    static const char *RX_TASK_TAG = "RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    uint8_t* data = (uint8_t*) calloc(RX_BUF_SIZE + 1, sizeof(uint8_t));


    
    while (1) {
        const int rxBytes = uart_read_bytes(UART_NUM, data, RX_BUF_SIZE, 1000 / portTICK_PERIOD_MS);
        //ESP_LOGI(TAG, "rxBytes: %d", rxBytes);
        //ESP_LOGI(TAG, "Buffer: %s", data);
        data[rxBytes] = 0;

        if(rxBytes > 4){
            for (int i=(rxBytes-5); i<=(rxBytes-2); i++) {
                //ESP_LOGI(TAG, "Data: %d", data[i]);
                if((data[i] == 21) || (data[i] == 15)){
                    validBEntry = 0;
                    break;
                }
                validBEntry = 1;
            }

        }
        if (rxBytes > 4 && screen == 1 && (codeCounter == 0) && BTN1_STATE && validBEntry) {
            //ESP_LOGI(TAG, "Printing Code");
            for (int i=(rxBytes-5); i<=(rxBytes-2); i++) {
               //ESP_LOGI(TAG, "Data: %d", data[i]);
                LCDwrite(data[i]);
                keyCode[codeCounter] = data[i];
                codeCounter++;
                col++;
            }
        } else if (rxBytes > 4 && screen == 1 && (codeCounter > 0) && BTN1_STATE &&validBEntry) {
            clear();
            if (BTN1_STATE) {
                printstr("Top Door Code: ");
            } else if (BTN2_STATE) {
                printstr("Bot Door Code: ");
            }
            codeCounter = 0;
            setCursor(6,1);
            col = 6;
            //ESP_LOGI(TAG, "Printing Code");
            for (int i=(rxBytes-5); i<=(rxBytes-2); i++) {
                //ESP_LOGI(TAG, "Data: %c", data[i]);
                LCDwrite(data[i]);
                keyCode[codeCounter] = data[i];
                codeCounter++;
                col++;
            }
        }
    }    
    free(data);
}

void app_main(void) {
    //Setup the WiFi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    // initializing everything
    configure_led();
    configure_button();
    i2c_init();
    setRGB(0, 255, 255);
    Barcode_init();

    //LEDs and Buttons Tasks
    xTaskCreate(&LED1, "LED1_Blink", 2048, NULL, 1, NULL);
    xTaskCreate(&Button1, "Button1Press", 2048, NULL, 1, NULL);

    xTaskCreate(&LED2, "LED2_Blink", 2048, NULL, 1, NULL);
    xTaskCreate(&Button2, "Button2Press", 2048, NULL, 1, NULL);
    
    
    //Motion sensor task
    configure_motion();
    xTaskCreate(&MotionSensor, "MotionDetector", 4096, NULL, 1, NULL);

    //Top Door and Bottom door Communication
    //Make sure to have common ground when connecting both boards

    
    configure_DoorCom();
    xTaskCreate(&TopDoorCom, "Top Door Communication", 2048, NULL, 1, NULL);
    xTaskCreate(&BotDoorCom, "Bottom Door Communication", 2048, NULL, 1, NULL);
    
    xTaskCreate(&MiddleDoorCom, "Middle Door Communication", 2048, NULL, 1, NULL);

    
    // LCD Display Task
    xTaskCreate(&Display, "LCD Display", 4096, NULL, 1, NULL);

    // Keypad Task
    xTaskCreate(&Keys, "Keypad", 2048, NULL, 1, NULL);

    // Barcode Recieve Task
    xTaskCreate(rx_task, "Barcode Recieve", 2048, NULL, 1, NULL);
}