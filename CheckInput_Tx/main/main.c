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

#define GPIO_INPUT_LEFT GPIO_NUM_17
#define GPIO_INPUT_RIGHT GPIO_NUM_21

static const char *TAG = "ESP_NOW";
uint8_t broadcastAddress[] = {0xEC, 0xC9, 0xFF, 0xE2, 0x5F, 0x60};

typedef struct button_status{
    bool left;
    bool right;
} button_status;

esp_now_peer_info_t peerInfo;

void init_gpio_input()
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_INPUT_LEFT)|(1ULL << GPIO_INPUT_RIGHT),
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
    button_status buttons;
    while(1){
        buttons.left = !gpio_get_level(GPIO_INPUT_LEFT);
        buttons.right = !gpio_get_level(GPIO_INPUT_RIGHT);

        esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)&buttons, sizeof(buttons));
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "Send error: %s", esp_err_to_name(result));
        }
        vTaskDelay(pdMS_TO_TICKS(100));
        ESP_LOGI(TAG, "Sending left=%d, right=%d", buttons.left, buttons.right);
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    init_wifi();
    init_gpio_input();


    if (esp_now_init() != ESP_OK) {
        ESP_LOGE(TAG, "Error initializing ESP-NOW");
        return;
    }
    esp_now_register_send_cb(OnDataSent); 
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
