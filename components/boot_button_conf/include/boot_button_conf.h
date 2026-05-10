#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#define BOOT_BUTTON_PIN GPIO_NUM_9

void func(void);
//static void button_isr_handler(void* arg);
void boot_button_config();
