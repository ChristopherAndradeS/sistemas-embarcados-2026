#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_idf_version.h"
#include "esp_chip_info.h"
#include "esp_log.h"
#include "esp_flash.h"
#include "esp_system.h"

TickType_t delay_ms(int milisseconds);

const char gEspModelName[CHIP_POSIX_LINUX + 1][32] = 
{
    "CHIP_ESP32",
    "CHIP_ESP32S2",
    "CHIP_ESP32S3",
    "CHIP_ESP32C3",
    "CHIP_ESP32C2",
    "CHIP_ESP32C6",
    "CHIP_ESP32H2",
    "CHIP_ESP32P4",
    "CHIP_ESP32C61",
    "CHIP_ESP32C5",
    "CHIP_ESP32H21",
    "CHIP_ESP32H4",
    "CHIP_POSIX_LINUX"
};

void app_main(void)
{
    static const char* TAG_1 = "[ PRATICA 1.1 ]";
    static const char* TAG_2 = "[ PRATICA 1.2 ]";
 
    ESP_LOGE(TAG_1, "ISSO AQUI É UM LOG DE ERRO");
    ESP_LOGW(TAG_1, "ISSO AQUI É UM LOG DE WARNING");
    ESP_LOGI(TAG_1, "ISSO AQUI É UM LOG DE INFO");

    ESP_LOGI(TAG_1, "\nDefinindo LOG_LEVEL como ESP_LOG_WARN\n");
    esp_log_level_set(TAG_1, ESP_LOG_WARN);

    ESP_LOGE(TAG_1, "LOG_LEVEL < ESP_LOG_ERRO : SEREI IMPRESSO NO SERIAL");
    ESP_LOGW(TAG_1, "LOG_LEVEL = ESP_LOG_WARN : SEREI IMPRESSO NO SERIAL");
    ESP_LOGI(TAG_1, "LOG_LEVEL > ESP_LOG_INFO : NÃO SEREI IMPRESSO NO SERIAL");

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    uint32_t flash_size;

    while(1)
    {
        ESP_LOGI(TAG_2, "--------------------------------------------------------------\n");
        
        ESP_LOGI(TAG_2, "Modelo do ESP: chip_info.model = %d  |  %s\n", chip_info.model, gEspModelName[chip_info.model]);
        ESP_LOGI(TAG_2, "Possui memória flash embutida ?         %s", chip_info.features & CHIP_FEATURE_EMB_FLASH ? "Sim" : "Não");
        ESP_LOGI(TAG_2, "Possui WIFI 2,4 GHz ?                   %s", chip_info.features & CHIP_FEATURE_WIFI_BGN ? "Sim" : "Não");
        ESP_LOGI(TAG_2, "Possui Bluetooth LE ?                   %s", chip_info.features & CHIP_FEATURE_BLE ? "Sim" : "Não");
        ESP_LOGI(TAG_2, "Possui Bluetooth Classic ?              %s", chip_info.features & CHIP_FEATURE_BT ? "Sim" : "Não");
        ESP_LOGI(TAG_2, "Possui IEEE 802.15.4 ?                  %s", chip_info.features & CHIP_FEATURE_IEEE802154 ? "Sim" : "Não");
        ESP_LOGI(TAG_2, "Possui PSRAM embutida ?                 %s", chip_info.features & CHIP_FEATURE_EMB_PSRAM ? "Sim\n" : "Não\n");

        ESP_LOGI(TAG_2, "Número de revisão: v%d.%d", chip_info.revision / 100, chip_info.revision % 100);

        ESP_LOGI(TAG_2, "Cores (núcleos): %d\n", chip_info.cores);

        if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) 
        {
            ESP_LOGE(TAG_2, "--------------------------------------------------------------\n");
            ESP_LOGE(TAG_2, "\nERRO: Não foi possível obter informação do tamanho da memória flash embutida!\n");
            ESP_LOGE(TAG_2, "--------------------------------------------------------------\n");
            break;
        }
        ESP_LOGW(TAG_2, "\n--------------------------------------------------------------\n");
    
        ESP_LOGW(TAG_2, "O tamanho da memória flash embutida é: %I64u MB", flash_size / (uint32_t)(1024 * 1024));
 
        ESP_LOGW(TAG_2, "\n--------------------------------------------------------------\n");
        
        ESP_LOGW(TAG_2, "ESP-IDF versão: %s\n", esp_get_idf_version());

        ESP_LOGW(TAG_2, "--------------------------------------------------------------\n");

        vTaskDelay(delay_ms(1000));
    }

    return;
}

TickType_t delay_ms(int milisseconds) 
{
    return (milisseconds / portTICK_PERIOD_MS);
}