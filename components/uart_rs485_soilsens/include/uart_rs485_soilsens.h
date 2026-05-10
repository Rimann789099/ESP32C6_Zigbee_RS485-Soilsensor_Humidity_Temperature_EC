#ifndef UART_HEADER
#define UART_HEADER  

#include <stdint.h>

void setup_uart();
void send_request(uint8_t *request);
void sens_polling(uint8_t* request_mess,char* data, int8_t length);
int32_t get_value_from_message(char* data, int8_t length);
void vTaskReadHum(void* dest);
void vTaskReadEC(void* dest);
void vTaskReadTemp(void* dest);
void vTaskHtcLogs(void* data_p);


 typedef struct {
  float* temperature;
  float* humidity;
  uint32_t* conductivity;
}  DataPointers;

extern DataPointers data_p;
extern float humidity;
extern float temperature; 
extern uint32_t conductivity; 


void activate_data_collector(void);

#endif // UART_HEADER
