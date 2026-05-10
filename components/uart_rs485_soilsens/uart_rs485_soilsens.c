#include <stdio.h>
#include "uart_rs485_soilsens.h"

#include "driver/uart.h"
//#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include <string.h>
//#include "hal/uart_types.h"
//#include "driver/usb_serial_jtag.h"


#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA   "\x1b[35m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_RESET   "\x1b[0m"

// Define TX and RX pins for UART (change if needed)
#define UART_TX_PIN    5//11
#define UART_RX_PIN    4//10xTaskCreate with args

#define RECIEVER_BUFF_SIZE 16

const uart_port_t uart_num = UART_NUM_1;


void setup_uart(){
  uart_config_t uart_cfg = {
    .baud_rate = 4800,
    .data_bits = UART_DATA_8_BITS, 
    .stop_bits = UART_STOP_BITS_1,
    .parity    = UART_PARITY_DISABLE,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
  };

  ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_cfg));
  ESP_ERROR_CHECK(uart_set_pin(uart_num, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
  
  const int uart_buffer_size = (2*1024);
  QueueHandle_t uart_queue; 
  ESP_ERROR_CHECK(uart_driver_install(uart_num , uart_buffer_size, uart_buffer_size, 10,&uart_queue, 0));

}




void send_request(uint8_t *request){
  uart_flush(uart_num);
  uart_write_bytes(uart_num, (uint8_t*)request, 8);
  ESP_ERROR_CHECK(uart_wait_tx_done(uart_num, 100/portTICK_PERIOD_MS));
}

void sens_polling(uint8_t* request_mess,char* data, int8_t length){
  send_request(request_mess);
  uart_read_bytes(uart_num, data, length, 100/portTICK_PERIOD_MS); 
}

int32_t get_value_from_message(char* data, int8_t length){
  uint32_t value = ((data[3])<<8)|(data[4]);
  //ESP_LOGI("SENS", "\tVAL: \t%d", value);
  return value; 
}

void vTaskReadHum(void* dest){
  //Sensor
  uint8_t soil_humidity_request[] = { 0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0x84, 0x0a };

  char* humidity_data_p = malloc(sizeof(char)*RECIEVER_BUFF_SIZE); 
  float* humidity_p = (float *) dest;

  while(1){
    sens_polling(soil_humidity_request, humidity_data_p, RECIEVER_BUFF_SIZE);

    *humidity_p = get_value_from_message(humidity_data_p, RECIEVER_BUFF_SIZE);
    *humidity_p/=10;
   // ESP_LOGI("SENS", ANSI_COLOR_BLUE"\tDEST_HUM_VAL: \t%f" ANSI_COLOR_RESET, *humidity_p);
    vTaskDelay(4200/portTICK_PERIOD_MS);
  }

  free(humidity_data_p);
}




void vTaskReadEC(void* dest){
  // SENSOR
  uint8_t soil_conductivity_request[] = { 0x01, 0x03, 0x00, 0x02, 0x00, 0x01, 0x25, 0xca };
  const char* conducitvity_data_p = malloc(sizeof(char)*RECIEVER_BUFF_SIZE); 
  uint32_t* conducitvity_p = (uint32_t *) dest;

  while(1){
    sens_polling(soil_conductivity_request, conducitvity_data_p, RECIEVER_BUFF_SIZE);
  // get_value<...>() waits for uint8t but gets uint32_t
    *conducitvity_p = get_value_from_message(conducitvity_data_p, RECIEVER_BUFF_SIZE);      
    vTaskDelay(3500/portTICK_PERIOD_MS);
  }

  free(conducitvity_data_p);
}

void vTaskReadTemp(void* dest){
  //SENSOR
  uint8_t soil_temperature_request[] = { 0x01, 0x03, 0x00, 0x01, 0x00, 0x01, 0xd5, 0xca };
  int8_t length = RECIEVER_BUFF_SIZE;
  const char* temperature_data_p = malloc(sizeof(char)*RECIEVER_BUFF_SIZE); 

  float* temperature_p = (float *) dest;

  while(1){

    sens_polling(soil_temperature_request, temperature_data_p, RECIEVER_BUFF_SIZE);
    //reinterpert memory
    *temperature_p = get_value_from_message(temperature_data_p, RECIEVER_BUFF_SIZE);
    *temperature_p/=10;
  
    //ESP_LOGI("TEMP_TASK", ANSI_COLOR_MAGENTA "\tDEST_TEMP_VAL: \t%f" ANSI_COLOR_MAGENTA, *temperature_p);
    vTaskDelay(15000/portTICK_PERIOD_MS);
  }
  free(temperature_data_p);
}


int8_t i = 1;
void sendTest() {
  i+=1;
  i%=10;
  //char test_request = {'a','b','c','0'+(char)i%2,'0'+(char)i,'f','g','\r'}; 
  //send_request(&test_request);
}


void vTaskGetSensorValue(void* dest){
  uint8_t* destination = (uint8_t *) dest;
  const char* temperature_data = malloc(sizeof(char)*16); 
  while(1){
    uart_flush(uart_num);
    sendTest();
    get_value_from_message(temperature_data, RECIEVER_BUFF_SIZE);
    vTaskDelay(1000/portTICK_PERIOD_MS);
    //ESP_LOGI("TASK", "\tDEST_VAL: \t%d", *destination);
  }
}



void vTaskHtcLogs(void* data_p){
 
  DataPointers* htc_data = (DataPointers*) data_p;
  while(1){
    ESP_LOGI("SENS", ANSI_COLOR_YELLOW"\t HUM: %f\t TEMP: %f\t EC: %lu" ANSI_COLOR_RESET, *(htc_data->humidity), *(htc_data->temperature), *(htc_data->conductivity));
    vTaskDelay(3000/portTICK_PERIOD_MS);
  }
}



DataPointers data_p;
float humidity;
float temperature; 
uint32_t conductivity;




void activate_data_collector(void)
{
  static uint8_t destination = 0;

  temperature = 5000;
  humidity = 6000;
  conductivity = 7000;
  
  data_p.temperature = &temperature,
  data_p.humidity=&humidity,
  data_p.conductivity = &conductivity,
   

  setup_uart(); 
  //xTaskCreate(vTaskGetSensorValue, "TaskSendData", 10000, &destination, 1, NULL );
  xTaskCreate(vTaskReadTemp, "TaskSendData", 10000, data_p.temperature, 1, NULL );
  xTaskCreate(vTaskReadHum, "TaskSendData", 10000, data_p.humidity, 1, NULL );
  xTaskCreate(vTaskReadEC, "TaskSendData", 10000, data_p.conductivity, 1, NULL );

  xTaskCreate(vTaskHtcLogs, "TaskSendLogs", 10000, &data_p, 1, NULL );

}
