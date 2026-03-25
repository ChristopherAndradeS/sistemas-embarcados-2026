#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"

TickType_t delay_ms(int milisseconds);

void app_main(void)
{
    static const char* TAG_1 = "[ PRATICA 1.1 ]";
    //static const char* TAG_2 = "[ PRATICA 1.2 ]";
 
    ESP_LOGE(TAG_1, "ISSO AQUI É UM LOG DE ERRO");
    ESP_LOGW(TAG_1, "ISSO AQUI É UM LOG DE WARNING");
    ESP_LOGI(TAG_1, "ISSO AQUI É UM LOG DE INFO");

    ESP_LOGI(TAG_1, "\nDefinindo LOG_LEVEL como ESP_LOG_WARN\n");
    esp_log_level_set(TAG_1, ESP_LOG_WARN);

    ESP_LOGE(TAG_1, "LOG_LEVEL < ESP_LOG_ERRO : SEREI IMPRESSO NO SERIAL");
    ESP_LOGW(TAG_1, "LOG_LEVEL = ESP_LOG_WARN : SEREI IMPRESSO NO SERIAL");
    ESP_LOGI(TAG_1, "LOG_LEVEL > ESP_LOG_INFO : NÃO SEREI IMPRESSO NO SERIAL");


    while(1)
    { 
        vTaskDelay(delay_ms(1000));
    }
}

TickType_t delay_ms(int milisseconds) 
{
    return (milisseconds / portTICK_PERIOD_MS);
}