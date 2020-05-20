
#include <string.h>
#include <errno.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "uart_e-puck2.h"
#include "gattc_gatts_coex.h"
#include "main_e-puck2.h"

/* The examples use WiFi configuration that you can set via project configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID      "YOUR_SSID"
#define EXAMPLE_ESP_WIFI_PASS      "YOUR_PASSWORD"
#define EXAMPLE_ESP_MAXIMUM_RETRY  20

#define PACKET_SIZE_SERVER2ROBOT (MAX_NUM_ROBOTS*2) // At most all robots id's (2 bytes per robot)
#define PACKET_SIZE_ROBOT2SERVER 3 // Only the id of the robot + add(1)|remove(2)
#define PORT 1000 // TCP port
#define SERVER_IP "192.168.1.8"

char rx_buf[PACKET_SIZE_SERVER2ROBOT], tx_buf[PACKET_SIZE_ROBOT2SERVER];

static const char *TAG = "wifi station";

static int s_retry_num = 0;

void closeSocket(int sock) {
	close(sock);
	return;
}

int8_t sendMsg(int sock, char* msg) {
	if(write(sock, msg, PACKET_SIZE_ROBOT2SERVER) < 0) {
		return -1;
	}
	return 0;
}

void alert_server(void) {
	uint16_t id = get_robot_id();
	tx_buf[0] = id&0xFF;
	tx_buf[1] = (id>>8);
	tx_buf[2] = 1; // Add my id to the DB sever list
}

void remove_from_server(void) {
	uint16_t id = get_robot_id();
	tx_buf[0] = id&0xFF;
	tx_buf[1] = (id>>8);
	tx_buf[2] = 2; // Remove my id from the DB sever list
}

static void tcp_client_task(void *pvParameters) {
	struct sockaddr_in temp;
	struct hostent *h;
	int sock;
	int err;
	/*
	float txTime = 0, throughput = 0;
	struct timeval startTime, exitTime;
	*/
	int i = 0;
	int totBytes = 0, ret = 0;
	uint8_t conn_state = 0;
	uint16_t id = 0;

    tcpip_adapter_init(); // Init TCP/IP stack.	
	
	temp.sin_family = AF_INET;
	temp.sin_port = htons(PORT);
	h = gethostbyname(SERVER_IP);
	if (h==0) {
		printf("Gethostbyname failed\n");
		exit(1);
	}
	bcopy(h->h_addr, &temp.sin_addr, h->h_length);

//	// socket option
//	int optval = 0;
//	socklen_t optlen;
//	optlen = sizeof(optval);
//
//	err = getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &optval, &optlen);
//	printf("getsockopt SO_RCVBUF = %d. err msg=%s\r\n", err, strerror(errno));
//	printf("Default receiving buffer size = %d\r\n", optval);
//	optval = 8192;
//	err = setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &optval, optlen);
//	printf("setsockopt SO_RCVBUF = %d. err msg=%s\r\n", err, strerror(errno));
//	err = getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &optval, &optlen);
//	printf("getsockopt SO_RCVBUF = %d. err msg=%s\r\n", err, strerror(errno));
//	printf("New receiving buffer size = %d\r\n", optval);
//	//err = getsockopt(sock, SOL_SOCKET, SO_SNDBUF, &optval, &optlen);
//    //printf("getsockopt SO_SNDBUF = %d. err msg=%s\r\n", err, strerror(errno));
//    //printf("Sending buffer size = %d\r\n", optval);

    //int one = 1;
    //err = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    //printf("setsockopt TCP_NODELAY = %d. err msg=%s\r\n", err, strerror(errno));

	memset(tx_buf, 0x00, PACKET_SIZE_ROBOT2SERVER);
	
	while(1) {
		switch(conn_state) {
			case 0: // Disconnected
				//turn_off_led3();
				sock = socket(AF_INET, SOCK_STREAM, 0);
				err = connect(sock, (struct sockaddr*) &temp, sizeof(temp));
				if(err == 0) {
					printf("Connected to server\r\n");
					conn_state = 1;
				} else {
					printf("Connection failed, retry...\r\n");
				}
				break;
				
			case 1: // Connected
				//turn_on_led3();
				/*
				gettimeofday(&startTime, NULL);
				*/
				if(sendMsg(sock, tx_buf) < 0) { // Connection lost, try reconnecting.
					printf("Connection lost, try reconnecting...\r\n");
					close(sock);
					conn_state = 0;
					break;
				}
				/*
				gettimeofday(&exitTime, NULL);
				txTime = (exitTime.tv_sec*1000000 + exitTime.tv_usec)-(startTime.tv_sec*1000000 + startTime.tv_usec);
				// (PACKET_SIZE_ROBOT2SERVER*NUM_PACKETS*8)/1'000'000 => Mbits
				// txTime/1'000'000 => seconds
				// ((PACKET_SIZE_ROBOT2SERVER*NUM_PACKETS*8)/1'000'000) / (txTime/1'000'000) = (PACKET_SIZE_ROBOT2SERVER*NUM_PACKETS*8)/txTime
				throughput = (float)(PACKET_SIZE_ROBOT2SERVER*8)/txTime;
				printf("\r\n");
				printf("%d bytes sent in %.3f ms\r\n", PACKET_SIZE_ROBOT2SERVER, txTime/1000.0);
				printf("Throughput = %.3f Mbit/s\r\n", throughput);
				*/
				totBytes = 0;
				memset(rx_buf, 0, PACKET_SIZE_SERVER2ROBOT);
				while(totBytes < PACKET_SIZE_SERVER2ROBOT) {
					ret = read(sock, rx_buf, PACKET_SIZE_SERVER2ROBOT-totBytes);
					if(ret < 0) {
						//printf("error recv: %d %s", ret, strerror(errno));
						printf("Connection lost, try reconnecting...\r\n");
						close(sock);
						conn_state = 0;
						break;
					} else if(ret == 0) {
						//printf("recv retv = 0");
					} else if(ret > 0) {
						totBytes += ret;
					}
				}
				for(i=0; i<MAX_NUM_ROBOTS; i++) {
					id = rx_buf[i*2]+rx_buf[i*2+1]*256;
					if(id == 0) {
						break;
					}
					robots_infected[i] = id;
				}
				robots_infected_index = i;
				infected_list_updated();
				//printf("rx = %d\r\n", rx_buf[0]+rx_buf[1]*256);
				break;
		}
		
		vTaskDelay(2000/portTICK_PERIOD_MS); // Server list updated every 2 seconds.
	}

	vTaskDelete(NULL);
	return;
}

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            ESP_LOGI(TAG, "Failed to connect to the AP");
        }
        ESP_LOGI(TAG,"connect to the AP fail");
		//turn_off_led1();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:%s",
                 ip4addr_ntoa(&event->ip_info.ip));
        s_retry_num = 0;
		//turn_on_led1();
    }
}

void wifi_init_sta()
{
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");

    tcpip_adapter_init();

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

	xTaskCreate(&tcp_client_task, "tcp_client_task", 2048, NULL, 5, NULL);

}

