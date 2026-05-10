idf_component_register(SRCS "htc_uart_zigbee.c"
                    INCLUDE_DIRS "."
	    	    REQUIRES esp_driver_uart uart_rs485_soilsens led_blink boot_button_conf nvs_flash )
