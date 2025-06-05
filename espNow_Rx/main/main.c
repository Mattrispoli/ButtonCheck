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


#define LED_ONE GPIO_NUM_18 //Button Off
#define LED_TWO GPIO_NUM_16 // Button on

static const char *TAG = "ESP_NOW Rx";

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *msg, int len) {
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac_addr[0], mac_addr[1], mac_addr[2],
             mac_addr[3], mac_addr[4], mac_addr[5]);
    ESP_LOGI(TAG, "Data: %.*s", len, msg);


    char safe_msg[MSG_MAX_LEN] = {0};
    snprintf(safe_msg, MSG_MAX_LEN, "%.*s", len < MSG_MAX_LEN ? len : MSG_MAX_LEN - 1, (char*)msg);
    xQueueSend(msg_queue, safe_msg, portMAX_DELAY);
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
        char rxMsg[MSG_MAX_LEN];
        if (xQueueReceive(msg_queue, rxMsg, portMAX_DELAY) == pdTRUE){
            if (strcmp(rxMsg, "999999") == 0){
                gpio_set_level(LED_ONE, 1);
                gpio_set_level(LED_TWO, 0);
                timeCheck = esp_timer_get_time();
            }
            else if(strcmp(rxMsg, "000000")==0){
                gpio_set_level(LED_ONE, 0);
                gpio_set_level(LED_TWO, 1);
                timeCheck = esp_timer_get_time();
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
            gpio_set_level(LED_ONE,0);
            gpio_set_level(LED_TWO,1);
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
    msg_queue = xQueueCreate(10, sizeof(char) * MSG_MAX_LEN);
    gpio_set_direction(LED_ONE, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_TWO, GPIO_MODE_OUTPUT);

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