
#ifndef LED_HEADER
#define LED_HEADER 

typedef enum {Red, Green, Blue, Yellow, White} Color; 

void configure_led(void);
void blink_led(Color color, int8_t led_state);
void vTaskSteeringBlink();

#endif
