/*

File    : main_e-puck2.c
Author  : Stefano Morgani
Date    : 19 May 2020
REV 1.0

Firmware to be run on the ESP32 of the e-puck2
*/

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/xtensa_api.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_attr.h"   
#include "esp_err.h"
#include "main_e-puck2.h"
#include "rgb_led_e-puck2.h"
#include "button_e-puck2.h"
#include "uart_e-puck2.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_timer.h"
#include "gattc_gatts_coex.h"
#include "wifi.h"

#define DP3T_TAG  "DP3T-DEMO"

#define BT_EVENT_BIT 	BIT0
#define WIFI_EVENT_BIT	BIT1

#define DAY_TIME_US 10000000 // 10 seconds
#define MOVE_SPEED 400

uint8_t bt_state = STATE_HEALTHY;
uint8_t robot_infected = 0;
int64_t start_t = -1;
uint16_t robots_heard[MAX_NUM_ROBOTS] = {0};
uint8_t robots_heard_index = 0;
uint8_t infected_list_updated_flag = 0;
uint16_t robots_infected[MAX_NUM_ROBOTS] = {0};
uint8_t robots_infected_index = 0;
uint8_t actor_mode = 0;

void set_robot_infected(void) {
	robot_infected = 1;
}

void bt_list_add(uint16_t id) {
	uint8_t i = 0;
	for(i=0; i<robots_heard_index; i++) {
		if(robots_heard[i] == id) { // Avoid duplicates.
			break;
		}
	}
	if(i==robots_heard_index) { // New robot detected
		robots_heard[robots_heard_index] = id;
		robots_heard_index++;
	}
}

void bt_list_reset(void) {
	robots_heard_index = 0;
}

void infected_list_update(uint16_t *list, uint8_t num) {
	infected_list_updated_flag = 1;
	memcpy(robots_infected, list, num*2);
	robots_infected_index = num;
}

void infected_list_updated(void) {
	infected_list_updated_flag = 1;
}

void app_main(void) {
	esp_err_t ret;
	bt_state = STATE_HEALTHY;
	uint8_t ind = 0, ind2 = 0;
	uint8_t go_quarantine = 0;
	uint8_t loop_2sec = 0;
	
	rgb_init();
	button_init();
	uart_init();
	
	vTaskDelay(100/portTICK_PERIOD_MS);
	turn_on_led1();
	uart_get_data_ptr(); // This is needed to update the robot state.
	uart_get_data_ptr();
	// During this delay the user need to select the mode chaging the selector position: 15 (tracing), 0 (patient zero), others positions (no tracing).
	vTaskDelay(3000/portTICK_PERIOD_MS);
	turn_off_led1();
	uart_get_data_ptr(); // This is needed to update the robot state.
	uart_get_data_ptr();
	if(get_selector() == 0) {
		ESP_LOGI(DP3T_TAG, "Patient zero mode\n");
		turn_on_front_led();
		actor_mode = MODE_PATIENT_ZERO;
	} else if(get_selector() == 15) {
		ESP_LOGI(DP3T_TAG, "Tracing mode\n");
		//turn_on_body_led();
		actor_mode = MODE_TRACING;
	} else {
		ESP_LOGI(DP3T_TAG, "No tracing mode\n");
		actor_mode = MODE_NO_TRACING;
	}
  
	// RGB handling task.
	xTaskCreate(rgb_task, "rgb_task", 2048, NULL, 4, NULL);    
	
	// Button handling task.
	xTaskCreate(button_task, "button_task", 2048, NULL, 4, NULL);  
  
	init_ble();

	ESP_LOGI(DP3T_TAG, "Start scanning\n");
	start_scanning();
	vTaskDelay(1000/portTICK_PERIOD_MS);	
	ESP_LOGI(DP3T_TAG, "Start advertising\n");
	if(actor_mode == MODE_PATIENT_ZERO) { // Patient zero start being contagious from the beginning
		set_bt_name(BT_NAME_CONTAGIOUS); // Advertisement will be started after setting device name.
		bt_state = STATE_CONTAGIOUS;
	} else {
		set_bt_name(BT_NAME_HEALTHY); // Advertisement will be started after setting device name.
		bt_state = STATE_HEALTHY;
	}
	
	//Initialize NVS
	ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
	
	if(actor_mode != MODE_NO_TRACING) { // No-tracing actors doesn't need connection with the server.
		wifi_init_sta();
	}
		
	enable_obstacle_avoidance(); // Enable obstacle avoidance on the robot.
	set_speed(MOVE_SPEED);
	
	while(1) {
		uart_get_data_ptr(); // This is needed to update the robot state.
		//ESP_LOGI(DP3T_TAG, "sel=%d", get_selector());
		
		// Print debugging informations about every 2 seconds.
		loop_2sec++;
		if(loop_2sec >= 8) {
			loop_2sec = 0;
			printf("BT list:\r\n");
			for(ind=0; ind<robots_heard_index; ind++) {
				printf("%d, ", robots_heard[ind]);
			}
			printf("\r\n");
			
			printf("Server list:\r\n");
			for(ind=0; ind<robots_infected_index; ind++) {
				printf("%d, ", robots_infected[ind]);
			}
			printf("\r\n");
		}
		
		
		switch(bt_state) {
			case STATE_HEALTHY:
				set_all_green(5);
				set_speed(MOVE_SPEED);
				// Check if the robot encountered some infected robots.
				if(infected_list_updated_flag == 1) {
					infected_list_updated_flag = 0;
					for(ind2=0; ind2<robots_infected_index; ind2++) {
						for(ind=0; ind<robots_heard_index; ind++) {
							if(robots_infected[ind2] == robots_heard[ind]) {
								go_quarantine = 1;
								break;
							}
						}
						if(go_quarantine == 1) { // Stop at the first match.
							break;
						}
					}
					if(go_quarantine == 1) {
						go_quarantine = 0;
						if(esp_random() < (UINT32_MAX>>2)) { // 25% probability of being infected once in quarantine.
							robot_infected = 1;
						}						
						stop_advertising();
						bt_state = STATE_QUARANTINE;
						break;
					}
				}		
				if(robot_infected == 1) {
					if(start_t < 0) {
						start_t = esp_timer_get_time();
					}
					if((esp_timer_get_time()-start_t) >= 3*DAY_TIME_US) {
						start_t = -1;
						set_bt_name(BT_NAME_CONTAGIOUS);
						bt_state = STATE_CONTAGIOUS;
					}
				}
				break;
				
			case STATE_CONTAGIOUS:
				set_all_yellow(5);
				set_speed(MOVE_SPEED);
				if(start_t < 0) {
					start_t = esp_timer_get_time();
				}
				if((esp_timer_get_time()-start_t) >= 2*DAY_TIME_US) {
					start_t = -1;
					stop_advertising();
					bt_state = STATE_INFECTED;
					alert_server();
				}			
				break;
				
			case STATE_INFECTED:
				set_all_red(5);
				set_speed(0);
				if(start_t < 0) {
					start_t = esp_timer_get_time();
				}
				if((esp_timer_get_time()-start_t) >= 7*DAY_TIME_US) {
					start_t = -1;
					set_bt_name(BT_NAME_IMMUNE);
					bt_state = STATE_IMMUNE;
					remove_from_server();
				}
				break;
				
			case STATE_QUARANTINE:
				set_all_blue(5);
				set_speed(0);
				if(start_t < 0) {
					start_t = esp_timer_get_time();
				}				
				if(robot_infected == 1) {
					if((esp_timer_get_time()-start_t) >= 5*DAY_TIME_US) {
						start_t = -1;
						bt_state = STATE_INFECTED;
						alert_server();
					}
				} else {
					if((esp_timer_get_time()-start_t) >= 7*DAY_TIME_US) {
						start_t = -1;
						set_bt_name(BT_NAME_HEALTHY);
						bt_state = STATE_HEALTHY;
						bt_list_reset();
					}
				}
				break;
				
			case STATE_IMMUNE:
				set_all_white(5);
				set_speed(MOVE_SPEED);
				break;
		}
		
		vTaskDelay(250/portTICK_PERIOD_MS);
		
	}
	
}
