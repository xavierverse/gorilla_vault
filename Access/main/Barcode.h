#ifndef __BARCODE_H__
#define __BARCODE_H__

#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"

#define TXD_PIN (GPIO_NUM_21)
#define RXD_PIN (GPIO_NUM_20)
#define UART_NUM UART_NUM_0

static const int RX_BUF_SIZE = 1024;

void Barcode_init(void);

int sendData(const char* logName, const char* data);

#endif