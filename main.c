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

#define GPIO_INPUT GPIO_NUM_17


static const char *TAG = "ESP_NOW";
uint8_t broadcastAddress[] = {0xEC, 0xC9, 0xFF, 0xE2, 0x5F, 0x60};


esp_now_peer_info_t peerInfo;

void init_gpio_input()
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_INPUT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,   
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  printf("\r\nLast Packet Send Status:\t");
  printf(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail\n");
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

void sendMsg(void *pvParameter){
    while(1){
        const char *message;
        if (gpio_get_level(GPIO_INPUT)!=0){
            message = "999999";
            
        }
        else{
            message = "000000";
        }
        printf("%s\n", message);
        ESP_LOGI(TAG, "GPIO level: %d", gpio_get_level(GPIO_INPUT));
        esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)message, strlen(message));
        if (result == ESP_OK) {
            ESP_LOGI(TAG, "Sent: %s", message);
        } else {
            ESP_LOGE(TAG, "Send error: %s", esp_err_to_name(result));
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    init_wifi();


    if (esp_now_init() != ESP_OK) {
        ESP_LOGE(TAG, "Error initializing ESP-NOW");
        return;
    }
    esp_now_register_send_cb(OnDataSent); 
    esp_now_peer_info_t peerInfo = {0};
    memcpy(peerInfo.peer_addr, broadcastAddress,6);
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;
    if (!esp_now_is_peer_exist(peerInfo.peer_addr)) {
        if (esp_now_add_peer(&peerInfo) != ESP_OK){
            ESP_LOGE(TAG, "Failed to add peer");
            return;
        }
    }
    xTaskCreate(sendMsg, "send_loop", 2048, NULL, 1, NULL);
    
    
    

}
