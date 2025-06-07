#include <stdio.h>
#include "esp_system.h"
#include "string.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#define MSG_QUEUE_LENGTH 10
#define MSG_MAX_LEN 64

QueueHandle_t msg_queue;

int64_t timeCheck;


#define LED_RIGHT GPIO_NUM_18 
#define LED_MID GPIO_NUM_16 
#define LED_LEFT GPIO_NUM_19



static const char *TAG = "ESP_NOW Rx";

typedef struct button_status{
    bool left;
    bool right;
} button_status;

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *msg, int len) {
    if (len != sizeof(button_status)) {
        ESP_LOGW(TAG, "Received unexpected data length: %d", len);
        return;
    }

    button_status buttons;
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac_addr[0], mac_addr[1], mac_addr[2],
             mac_addr[3], mac_addr[4], mac_addr[5]);


    memcpy(&buttons, msg, sizeof(button_status));  // Safe copy

    xQueueSend(msg_queue, &buttons, portMAX_DELAY);

}




void init_wifi(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void receiveMsg(void *pvParameter){
    while(1){
        button_status recv_status;
        if (xQueueReceive(msg_queue, &recv_status, portMAX_DELAY) == pdTRUE){
            if (recv_status.left == 0 && recv_status.right ==0){
                gpio_set_level(LED_MID, 0);
                gpio_set_level(LED_LEFT, 0);
                gpio_set_level(LED_RIGHT, 0);
            }else if (recv_status.left == 1 && recv_status.right ==1){
                gpio_set_level(LED_MID, 1);
                gpio_set_level(LED_LEFT, 0);
                gpio_set_level(LED_RIGHT, 0);
                timeCheck = esp_timer_get_time();
            }else if(recv_status.left == 1){
                gpio_set_level(LED_MID, 0);
                gpio_set_level(LED_LEFT, 1);
                gpio_set_level(LED_RIGHT, 0);
                timeCheck = esp_timer_get_time();
            }else if(recv_status.right == 1){
                gpio_set_level(LED_MID, 0);
                gpio_set_level(LED_LEFT, 0);
                gpio_set_level(LED_RIGHT, 1);
            }
        }
    }
}
void status_check(void *pvParameter){
    bool lost = false;
    while(1){
        if (esp_timer_get_time()-timeCheck>3000000){
            if(!lost){
            ESP_LOGI(TAG, "Connection Lost\n");
            gpio_set_level(LED_MID,0);
            gpio_set_level(LED_LEFT,0);
            gpio_set_level(LED_RIGHT,0);

            lost = true;
            

            }
        } else{
            lost = false;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    timeCheck = esp_timer_get_time();
    ESP_ERROR_CHECK(nvs_flash_init());
    msg_queue = xQueueCreate(10, sizeof(button_status));
    gpio_set_direction(LED_MID, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_LEFT, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_RIGHT, GPIO_MODE_OUTPUT);


    init_wifi();

    if (esp_now_init() != ESP_OK){
        ESP_LOGE(TAG, "Error initializing ESP NOW");
        return;
    }
    esp_now_register_recv_cb(OnDataRecv);

    ESP_LOGI(TAG, "ESP-NOW Receiver Initialized");

    xTaskCreate(receiveMsg, "Message",2048, NULL, 5, NULL);
    xTaskCreate(status_check, "Check Connection", 2048, NULL, 4, NULL);
}