#include <stdio.h>
#include "led_blink.h"
#include "driver/gpio.h"
#include "led_strip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#define BLINK_GPIO  8   //For Esp32c6 CONFIG_BLINK_GPIO=8


static led_strip_handle_t led_strip;



void configure_led(void)
{
    /* LED strip initialization with the GPIO and pixels number*/
    led_strip_config_t strip_config = {
        .strip_gpio_num = BLINK_GPIO,
        .max_leds = 1, // at least one LED on board
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

    led_strip_clear(led_strip);
}


void blink_led( Color color, int8_t led_state)
{
    /* If the addressable LED is enabled */
    if (led_state) {
        /* Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each color */
    switch (color) {
      case Red:
        led_strip_set_pixel(led_strip, 0, 100, 1, 1);
        break;
      case Green:
        led_strip_set_pixel(led_strip, 0, 1, 100, 1);
        break;
      case Blue:
        led_strip_set_pixel(led_strip, 0, 1, 1, 100);
        break;
      case Yellow:
        led_strip_set_pixel(led_strip, 0, 100, 100, 1);
        break;
      case White:
        led_strip_set_pixel(led_strip, 0, 255, 255, 255);
        break;
    }        
        /* Refresh the strip to send data */
        led_strip_refresh(led_strip);
    } else {
        /* Set all LED off to clear all pixels */
        led_strip_clear(led_strip);
    }
}



void vTaskSteeringBlink(){
  static int8_t led_state = 0; 
  //Color led_color = Blue;
  if (led_state == 0){
    blink_led(Blue, 0);
  } else {
    blink_led(Blue, 1);
  }
  vTaskDelay(1000/portTICK_PERIOD_MS);
}


