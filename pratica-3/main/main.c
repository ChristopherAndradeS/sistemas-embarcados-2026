#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_idf_version.h"
#include "esp_chip_info.h"
#include "esp_log.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"

#define GPIO_INPUT_IO_21   (21)
#define GPIO_INPUT_IO_22   (22)
#define GPIO_INPUT_IO_23   (23)

#define GPIO_OUTPUT_IO_2   (GPIO_NUM_2)

#define GPIO_INPUT_PIN_SEL (( 1ULL << GPIO_INPUT_IO_21 ) | \
                            ( 1ULL << GPIO_INPUT_IO_22 ) | \
                            ( 1ULL << GPIO_INPUT_IO_23 ))
                            
#define GPIO_OUTPUT_PIN_SEL ( 1ULL << GPIO_OUTPUT_IO_2 )

#define ESP_INTR_FLAG_DEFAULT 0

TickType_t delay_ms(int milisseconds);

void PrintEspInfo();

uint64_t GetMilisFromMHz(uint64_t MHz);

static QueueHandle_t gpio_queue = NULL;
static QueueHandle_t timer_queue = NULL;

const char gEspModelName[CHIP_POSIX_LINUX][32] = 
{
    "INVALID_NAME_CHIP",
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

typedef struct 
{
    uint64_t days;
    uint64_t hours;
    uint64_t minutes;
    uint64_t seconds;
    uint64_t milis;
    uint64_t alarm_value;
    uint64_t count_value;
} cclock_t;

static const char* TAG_1 = "[ PRATICA 1.1 ]";
static const char* TAG_2 = "[ PRATICA 1.2 ]";
static const char* TAG_3 = "[ PRATICA 2.0 ]";
static const char* TAG_4 = "[ PRATICA 3.0 ]";
static const char* TAG_5 = "[ RELÓGIO ]";

static void IRAM_ATTR gpio_handle(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_queue, &gpio_num, NULL);
}

static void gpio_task(void* arg)
{
    uint32_t io_num;

    gpio_config_t input_config = 
    {
        .pin_bit_mask = GPIO_INPUT_PIN_SEL,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };

    gpio_config_t output_config = 
    {
        .pin_bit_mask = GPIO_OUTPUT_PIN_SEL,
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };

    if(gpio_config(&input_config) != ESP_OK)
        ESP_LOGE(TAG_3, "ERRO: Não foi possível configurar gpio's!\n");    
    else
        ESP_LOGI(TAG_3, "GPIO INPUT: Configuração realizada com sucesso.");

    if(gpio_config(&output_config) != ESP_OK)
        ESP_LOGE(TAG_3, "ERRO: Não foi possível configurar gpio's!\n");
    else
        ESP_LOGI(TAG_3, "GPIO OUTPUT: Configuração realizada com sucesso.");
    
    gpio_queue = xQueueCreate(10, sizeof(uint32_t));

    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    
    gpio_isr_handler_add(GPIO_INPUT_IO_21, gpio_handle, (void*) GPIO_INPUT_IO_21);
    gpio_isr_handler_add(GPIO_INPUT_IO_22, gpio_handle, (void*) GPIO_INPUT_IO_22);
    gpio_isr_handler_add(GPIO_INPUT_IO_23, gpio_handle, (void*) GPIO_INPUT_IO_23);

    while(1)
    {
        if(xQueueReceive(gpio_queue, &io_num, portMAX_DELAY)) 
        {
            vTaskDelay(delay_ms(50));
            
            if(gpio_get_level(io_num)) continue;

            ESP_LOGI(TAG_3, "O botão GPIO_%d foi precionado: binário 0x%08x", io_num , io_num);
            
            switch(io_num)
            {
                case ( GPIO_INPUT_IO_21 ):
                    gpio_set_level(GPIO_OUTPUT_IO_2, 1);
                    ESP_LOGW(TAG_3, "LED Acesso!");
                    break;

                case ( GPIO_INPUT_IO_22 ):
                    gpio_set_level(GPIO_OUTPUT_IO_2, 0);
                    ESP_LOGW(TAG_3, "LED Apagado!");
                    break;
                
                case ( GPIO_INPUT_IO_23 ):
                    gpio_set_level(GPIO_OUTPUT_IO_2, gpio_get_level(GPIO_OUTPUT_IO_2) ? 0 : 1);
                    ESP_LOGW(TAG_3, "Toggle do LED! | Estado: %s", gpio_get_level(GPIO_OUTPUT_IO_2) ? "ON" : "OFF");
                    break;
            }
        }
    }
}

static bool IRAM_ATTR OnMilisUpdate(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_data)
{
    BaseType_t high_task_awoken = pdFALSE;
    QueueHandle_t queue = (QueueHandle_t)user_data;

    gptimer_alarm_config_t alarm_config = 
    {
        .alarm_count = edata->alarm_value + 100000,
    };

    gptimer_set_alarm_action(timer, &alarm_config);

    if(!(edata->alarm_value % 1000000))
    { 
        cclock_t Clock;

        Clock.alarm_value = edata->alarm_value;
        Clock.count_value = edata->count_value;
    
        xQueueSendFromISR(queue, &Clock, &high_task_awoken);
    }

    return (high_task_awoken == pdTRUE);
}

static void timer_task(void* arg)
{
    cclock_t Clock;
    
    timer_queue = xQueueCreate(10, sizeof(Clock));

    if(!timer_queue) 
    {
        ESP_LOGE(TAG_4, "ERRO: Não foi possível criar a fila corretamente.");
        return;
    }
    
    gptimer_handle_t gptimer = NULL;

    gptimer_config_t timer_config = 
    {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000, // 1MHz
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

    gptimer_event_callbacks_t callback = 
    {
        .on_alarm = OnMilisUpdate,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &callback, timer_queue));

    ESP_ERROR_CHECK(gptimer_enable(gptimer));
    
    gptimer_alarm_config_t alarm_config = 
    {
        .alarm_count = 100000, 
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));
    ESP_ERROR_CHECK(gptimer_start(gptimer));

    while(1)
    {
        if(xQueueReceive(timer_queue, &Clock, pdMS_TO_TICKS(1500)))   
        {
            Clock.milis    = GetMilisFromMHz(Clock.alarm_value);
            Clock.days     = Clock.milis / (1000 * 60 * 60 * 24);
            Clock.hours    = Clock.milis / (1000 * 60 * 60) % 24;
            Clock.minutes  = Clock.milis / (1000 * 60) % 60;
            Clock.seconds  = Clock.milis / (1000) % 60;

            ESP_LOGI(TAG_5, "Dias: %02llu | %02llu:%02llu:%02llu", 
                    Clock.days, Clock.hours, Clock.minutes, Clock.seconds);
        }
        else  
            ESP_LOGW(TAG_4, "Aviso: Contagem perdida!");
    }
}

void app_main(void)
{
    esp_log_level_set(TAG_1, ESP_LOG_NONE);
    esp_log_level_set(TAG_2, ESP_LOG_NONE);
    esp_log_level_set(TAG_3, ESP_LOG_NONE);

    PrintEspInfo();

    xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 1, NULL);

    xTaskCreate(timer_task, "timer_task", 2048, NULL, 1, NULL);

    while(1)
    {
        vTaskDelay(delay_ms(1000));
    }

    return;
}

void PrintEspInfo()
{
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
        return;
    }

    ESP_LOGW(TAG_2, "\n--------------------------------------------------------------\n");

    ESP_LOGW(TAG_2, "O tamanho da memória flash embutida é: %d B | %d MB", flash_size, flash_size / (uint32_t)(1024 * 1024));

    ESP_LOGW(TAG_2, "\n--------------------------------------------------------------\n");
    
    ESP_LOGW(TAG_2, "ESP-IDF versão: %s\n", esp_get_idf_version());

    ESP_LOGW(TAG_2, "--------------------------------------------------------------\n");
}

TickType_t delay_ms(int milisseconds) 
{
    return (milisseconds / portTICK_PERIOD_MS);
}

uint64_t GetMilisFromMHz(uint64_t MHz)
{
    return (uint64_t) (MHz / 1000); 
}
