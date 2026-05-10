#include "uart_rs485_soilsens.h"

#include <stdio.h>
#include "htc_uart_zigbee.h"
#include "led_blink.h"
#include "boot_button_conf.h"


//Logs 
#include "esp_log.h"
#include "esp_check.h"

#include "nvs_flash.h"
#include "string.h"

//OS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#include "esp_zigbee_core.h"
#include <time.h>
#include <sys/time.h>



/*---------Global Definitions---------*/
static char manufacturer[16], model[16], firmware_version[16];
bool time_updated = false, connected = false, DEMO_MODE = true; /*< DEMO_MDE disable all real sensors and send fake data*/
char strftime_buf[64];

extern float temperature; uint16_t temperature_int = 0; 
extern float humidity; uint16_t humidity_int = 0; 
extern uint32_t conductivity; uint16_t conductivity_int = 0; 


static const char *TAG = "HTC_SENSOR";



static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    ESP_ERROR_CHECK(esp_zb_bdb_start_top_level_commissioning(mode_mask));
}

static void get_rtc_time()
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%a %H:%M:%S", &timeinfo);    
}





/* Manual reporting atribute to coordinator */
static void reportAttribute(uint8_t endpoint, uint16_t clusterID, uint16_t attributeID, void *value, uint8_t value_length)
{   
    uint16_t attributes[] = {attributeID};
    esp_zb_zcl_report_attr_cmd_t cmd = {
        .zcl_basic_cmd = {
            .dst_addr_u.addr_short = 0x0000,
            .dst_endpoint = endpoint,
            .src_endpoint = endpoint,
        },
        .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .clusterID = clusterID,
        //.attr_number = sizeof(attributes) / sizeof(uint16_t);
        .attributeID = attributeID,
        .direction = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
    };
    esp_zb_zcl_attr_t *value_r = esp_zb_zcl_get_attribute(endpoint, clusterID, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, attributeID);
    memcpy(value_r->data_p, value, value_length);
    esp_zb_zcl_report_attr_cmd_req(&cmd);
}



/* Task for update attribute value */
void update_attribute()
{
    while(1)
    {
        if (connected)
        {
            /*Data Conversion*/
            humidity_int = (uint16_t) (humidity*100);
            temperature_int = (uint16_t) (temperature*100);
            conductivity_int = (uint16_t) conductivity;   
            /* Write new temperature value */
            esp_zb_zcl_status_t state_tmp = esp_zb_zcl_set_attribute_val(SENSOR_ENDPOINT, ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID, &temperature_int, false);
            
            /* Check for error */
            if(state_tmp != ESP_ZB_ZCL_STATUS_SUCCESS)
            {
                ESP_LOGE(TAG, "Setting temperature attribute failed! ERR: %X", state_tmp);
            }
            
            /* Write new humidity value */
            esp_zb_zcl_status_t state_hum = esp_zb_zcl_set_attribute_val(SENSOR_ENDPOINT, ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID, &humidity_int, false);
           
            /* Check for error */
           if(state_hum != ESP_ZB_ZCL_STATUS_SUCCESS)
            {
                ESP_LOGI("DEBUG", "Setting humidity: value=%d", humidity_int);
                ESP_LOGE(TAG, "Setting humidity attribute failed! ERR: %X",  state_hum);
            }

            /* Write new Conductiveness value */
            esp_zb_zcl_status_t state_ec = esp_zb_zcl_set_attribute_val(SENSOR_ENDPOINT, ESP_ZB_ZCL_CLUSTER_ID_EC_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_EC_MEASUREMENT_MEASURED_VALUE_ID, &conductivity_int, false);
            
            /* Check for error */
            if(state_ec != ESP_ZB_ZCL_STATUS_SUCCESS)
            {
                ESP_LOGE(TAG, "Setting Conductiveness attribute failed! ERR: %X", state_ec);
            }
  
        }
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }
}


static esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG, "Received message: error status(%d)",
                        message->info.status);
    ESP_LOGI(TAG, "Received message: endpoint(%d), cluster(0x%x), attribute(0x%x), data size(%d)", message->info.dst_endpoint, message->info.cluster,
             message->attribute.id, message->attribute.data.size);
    if (message->info.dst_endpoint == SENSOR_ENDPOINT) {
        switch (message->info.cluster) {
        case ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY:
            ESP_LOGI(TAG, "Identify pressed");
            break;
        default:
            ESP_LOGI(TAG, "Message data: cluster(0x%x), attribute(0x%x)  ", message->info.cluster, message->attribute.id);
        }
    }
    return ret;
}


static esp_err_t zb_read_attr_resp_handler(const esp_zb_zcl_cmd_read_attr_resp_message_t *message)
{
    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG, "Received message: error status(%d)",
                        message->info.status);

    esp_zb_zcl_read_attr_resp_variable_t *variables = message->variables;

    ESP_LOGI(TAG, "Read attribute response: status(%d), cluster(0x%x), attribute(0x%x), type(0x%x), value(%d)", message->info.status,
             message->info.cluster, variables->attribute.id, variables->attribute.data.type,
             variables->attribute.data.value ? *(uint8_t*) variables->attribute.data.value : 0);

    if (message->info.dst_endpoint == SENSOR_ENDPOINT) {
        switch (message->info.cluster) {
        case ESP_ZB_ZCL_CLUSTER_ID_TIME:
            ESP_LOGI(TAG, "Server time recieved %lu", *(uint32_t*) variables->attribute.data.value);
            struct timeval tv;
            tv.tv_sec = *(uint32_t*) variables->attribute.data.value + 946684800 - 1080; //after adding OTA cluster time shifted to 1080 sec... strange issue ... 
            settimeofday(&tv, NULL);
            time_updated = true;
            break;
        default:
            ESP_LOGI(TAG, "Message data: cluster(0x%x), attribute(0x%x)  ", message->info.cluster, variables->attribute.id);
        }
    }
    return ESP_OK;
}

static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    esp_err_t ret = ESP_OK;
    switch (callback_id) {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
        ret = zb_attribute_handler((esp_zb_zcl_set_attr_value_message_t *)message);
        break;
    case ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID:
        ret = zb_read_attr_resp_handler((esp_zb_zcl_cmd_read_attr_resp_message_t *)message);
        break;
    default:
        ESP_LOGW(TAG, "Receive Zigbee action(0x%x) callback", callback_id);
        break;
    }
    return ret;
}


void read_server_time()
{ 
    uint16_t time_attributes[] = {ESP_ZB_ZCL_ATTR_TIME_LOCAL_TIME_ID};
    esp_zb_zcl_read_attr_cmd_t read_req;
    read_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    read_req.attr_field = time_attributes;
    read_req.attr_number = sizeof(time_attributes) / sizeof(uint16_t); 
    read_req.clusterID = ESP_ZB_ZCL_CLUSTER_ID_TIME;
    read_req.zcl_basic_cmd.dst_endpoint = 1;
    read_req.zcl_basic_cmd.src_endpoint = 1;
    read_req.zcl_basic_cmd.dst_addr_u.addr_short = 0x0000;
    esp_zb_zcl_read_attr_cmd_req(&read_req);
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p       = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    esp_zb_zdo_signal_leave_params_t *leave_params = NULL;
    esp_zb_zdo_device_unavailable_params_t *unavaliable_params = NULL;
    switch (sig_type) {
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
    case ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS:
        if (err_status != ESP_OK){
            ESP_LOGW(TAG, "Network is closed for joining new devices. Stack %s failure with %s status",esp_zb_zdo_signal_to_string(sig_type), esp_err_to_name(err_status));
            blink_led(Yellow, 1);
        }
    case ESP_ZB_BDB_SIGNAL_STEERING:
        
        if (err_status != ESP_OK) {

            
            connected = false;
            ESP_LOGW(TAG, "Stack %s failure with %s status, steering",esp_zb_zdo_signal_to_string(sig_type), esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
            blink_led(Blue, 1);
        } else {
            /* device auto start successfully and on a formed network */
            connected = true;
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d)",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel());
            read_server_time();

            /* Additional LED Signal*/
            blink_led(Green, 1);
        }
        break;
    case ESP_ZB_ZDO_SIGNAL_LEAVE:
        leave_params = (esp_zb_zdo_signal_leave_params_t *)esp_zb_app_signal_get_params(p_sg_p);

        ESP_LOGI(TAG, "Device Left Network");
        if (leave_params->leave_type == ESP_ZB_NWK_LEAVE_TYPE_RESET) {
            ESP_LOGI(TAG, "Reset device");
            esp_zb_factory_reset();
        } 
        blink_led(Red, 1);      
        break;
    case ESP_ZB_ZDO_DEVICE_UNAVAILABLE:
        unavaliable_params = (esp_zb_zdo_device_unavailable_params_t *)esp_zb_app_signal_get_params(p_sg_p);
        ESP_LOGW(TAG, "Destination device not avaliable. Destination PAN ID: 0x%04hx ",unavaliable_params->short_addr );
        blink_led(Red, 1);      
        break;
    case ESP_ZB_NLME_STATUS_INDICATION:
        //blink_led(Yellow, 1);
        esp_zb_zdo_signal_nwk_status_indication_params_t *params = (esp_zb_zdo_signal_nwk_status_indication_params_t *)esp_zb_app_signal_get_params(p_sg_p);

        // Now you can access fields in params, for example:
        ESP_LOGI("ZB_APP", "NLME Status: %u", params->status);
        break;
    default:        
        //blink_led(White, 1);
        ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
                 esp_err_to_name(err_status));
        break;
    }
}







static void set_zcl_string(char *buffer, char *value) 
{
    buffer[0] = (char) strlen(value);
    memcpy(buffer + 1, value, buffer[0]);
}

static void esp_zb_task(void *PwParameters){
    uint16_t undefined_value;
    undefined_value = 0x8000;   

   /* initialize Zigbee stack */
     esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZR_CONFIG();
      esp_zb_init(&zb_nwk_cfg);
  
    /* basic cluster create with fully customized */
    set_zcl_string(manufacturer, MANUFACTURER_NAME);
    set_zcl_string(model, MODEL_NAME);
    set_zcl_string(firmware_version, FIRMWARE_VERSION);
    uint8_t dc_power_source;
    dc_power_source = 4;
    esp_zb_attribute_list_t *esp_zb_basic_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_BASIC);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, manufacturer);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, model);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_SW_BUILD_ID, firmware_version);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID, &dc_power_source);  // < DC source. 
     
    /* identify cluster create with fully customized */
    uint8_t identyfi_id;
    identyfi_id = 0;
    esp_zb_attribute_list_t *esp_zb_identify_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY);
    esp_zb_identify_cluster_add_attr(esp_zb_identify_cluster, ESP_ZB_ZCL_CMD_IDENTIFY_IDENTIFY_ID, &identyfi_id);

    /* Temperature cluster */
    esp_zb_temperature_meas_cluster_cfg_t temperature_cfg = {
      .measured_value = 0,
      .min_value = 0,
      .max_value = 10000,
    };

    esp_zb_attribute_list_t *esp_zb_temperature_meas_cluster = esp_zb_temperature_meas_cluster_create(&temperature_cfg);
      /* Conductiveness cluster */
    esp_zb_ec_measurement_cluster_cfg_t ec_cfg = {
      .measured_value = 0,
      .min_measured_value = 0,
      .max_measured_value = 10000,
    };
    
    esp_zb_attribute_list_t *esp_zb_conducitivity_meas_cluster = esp_zb_ec_measurement_cluster_create(&ec_cfg);
   
 
    /* Humidity cluster */
    esp_zb_humidity_meas_cluster_cfg_t humidity_cfg = {
      .measured_value = 0,
      .min_value = 0,
      .max_value = 10000,
  };
    esp_zb_attribute_list_t *esp_zb_humidity_meas_cluster = esp_zb_humidity_meas_cluster_create(&humidity_cfg);

    /* Create full cluster list enabled on device */
    esp_zb_cluster_list_t *esp_zb_cluster_list = esp_zb_zcl_cluster_list_create();
    esp_zb_cluster_list_add_basic_cluster(esp_zb_cluster_list, esp_zb_basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(esp_zb_cluster_list, esp_zb_identify_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_temperature_meas_cluster(esp_zb_cluster_list, esp_zb_temperature_meas_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_humidity_meas_cluster(esp_zb_cluster_list, esp_zb_humidity_meas_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_ec_measurement_cluster(esp_zb_cluster_list, esp_zb_conducitivity_meas_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);


    
    esp_zb_ep_list_t *esp_zb_ep_list = esp_zb_ep_list_create();
      
    esp_zb_endpoint_config_t sensor_ep_cfg =  
    { .endpoint=SENSOR_ENDPOINT, 
      .app_profile_id=ESP_ZB_AF_HA_PROFILE_ID, 
      .app_device_id=ESP_ZB_HA_CUSTOM_ATTR_DEVICE_ID,//ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID,
      .app_device_version=0
    };
    esp_zb_ep_list_add_ep(esp_zb_ep_list, esp_zb_cluster_list, sensor_ep_cfg);
   
  
    esp_zb_device_register(esp_zb_ep_list);
    esp_zb_core_action_handler_register(zb_action_handler);
    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);
    ESP_ERROR_CHECK(esp_zb_start(true));
    esp_zb_stack_main_loop();
}


static void button_isr_handler(void* arg) {
    esp_zb_bdb_reset_via_local_action();
    ESP_EARLY_LOGI(TAG, "BOOT button pressed!");
    
}


void app_main(void)
{
  boot_button_config();
  gpio_install_isr_service(0);
  gpio_isr_handler_add(BOOT_BUTTON_PIN, button_isr_handler, NULL);
  configure_led();

  esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
  };
  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_zb_platform_config(&config));

  activate_data_collector();
  xTaskCreate(update_attribute, "Update_attribute_value", 4096, NULL, 5, NULL);
  xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 6, NULL);
}
