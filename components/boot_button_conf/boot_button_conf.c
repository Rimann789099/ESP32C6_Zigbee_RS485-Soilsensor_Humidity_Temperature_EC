#include <stdio.h>

#include "boot_button_conf.h"


/**
static void button_isr_handler(void* arg) {

    ESP_EARLY_LOGI(TAG, "BOOT button pressed!");
}
*/

void boot_button_config(){
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BOOT_BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&io_conf);

}


