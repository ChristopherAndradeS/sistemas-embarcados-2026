#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/lock.h>
#include <sys/param.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_idf_version.h"
#include "esp_chip_info.h"
#include "esp_log.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "driver/ledc.h"
#include "driver/i2c_master.h"
#include "soc/soc_caps.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "lvgl.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include "mqtt_client.h"
#include "pid_ctrl.h"

#define GPIO_OUTPUT_IO_17   (26)
#define GPIO_OUTPUT_IO_26   (17)

#define PIN_SDA                 (19)
#define PIN_SCL                 (18)
#define PIN_NUM_RST             (-1)
#define I2C_BUS_PORT            (0)
#define I2C_HW_ADDR             (0x3C)
#define LCD_PIXEL_CLOCK_HZ      (400 * 1000)
#define LCD_CMD_BITS            (8)
#define LCD_V_RES               (64)
#define LCD_H_RES               (128)
#define LVGL_PALETTE_SIZE       (8)
#define LVGL_TASK_STACK_SIZE    (4 * 1024)
#define LVGL_TASK_PRIORITY      (2)
#define LVGL_TICK_PERIOD_MS     (5)
#define LVGL_TASK_MAX_DELAY_MS  (500)
#define LVGL_TASK_MIN_DELAY_MS  (1000 / CONFIG_FREERTOS_HZ)

#define UPDATE_RATE_MS          (100)
#define TIMER_RESOLUTION_HZ     (1000000)
#define UPDATE_RATE_TICKS       ((TIMER_RESOLUTION_HZ / 1000) * UPDATE_RATE_MS)

#define MIN_TEMPERATURE         (25.0f)
#define MAX_TEMPERATURE         (100.0f)
#define PID_OUTPUT_MIN         (0.0f)
#define PID_OUTPUT_MAX         (100.0f)
#define PWM_MAX_DUTY            (8191)
#define COOLER_DEADBAND_C       (1.0f)
#define COOLER_SCALE_PERCENT    (20.0f)

static uint8_t oled_buffer[LCD_H_RES * LCD_V_RES / 8];
static _lock_t lvgl_api_lock;

static lv_obj_t *label_setpoint;
static lv_obj_t *label_temperature;
static lv_obj_t *label_speed;

static QueueHandle_t timer_queue = NULL;
static QueueHandle_t pwm_queue = NULL;
static QueueHandle_t controller_queue = NULL;

static SemaphoreHandle_t semaphore_adc = NULL; 

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

typedef struct 
{
    uint16_t speed;
    float temperature;
    float setpoint;
} sensor_data_t;

typedef struct
{
    uint16_t cooler;
    uint16_t heater;
} duty_t;

typedef struct
{
    float setpoint;
    float temperature;
    pid_ctrl_block_handle_f_t pid_handle;
} controller_data_t;

static controller_data_t farm_controller =
{
    .setpoint = 38.0f,
    .temperature = 0.0f,
    .pid_handle = NULL,
};

static float current_heater_percent = 0.0f;
static float current_cooler_percent = 0.0f;

static const char* TAG_FARM = "[ GRANJA ]";

sensor_data_t farm_data;

static bool IRAM_ATTR OnMilisUpdate(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_data)
{
    BaseType_t high_task_awoken = pdFALSE;
    QueueHandle_t queue = (QueueHandle_t)user_data;

    gptimer_alarm_config_t alarm_config = 
    {
        .alarm_count = edata->alarm_value + UPDATE_RATE_TICKS,
    };

    gptimer_set_alarm_action(timer, &alarm_config);

    cclock_t Clock;

    Clock.alarm_value = edata->alarm_value;
    Clock.count_value = edata->count_value;
    
    xQueueSendFromISR(queue, &Clock, &high_task_awoken);

    return (high_task_awoken == pdTRUE);
}

static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t io_panel, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_display_t *disp = (lv_display_t *)user_ctx;
    lv_display_flush_ready(disp);
    return false;
}

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(disp);

    px_map += LVGL_PALETTE_SIZE;

    uint16_t hor_res = lv_display_get_physical_horizontal_resolution(disp);
    int x1 = area->x1;
    int x2 = area->x2;
    int y1 = area->y1;
    int y2 = area->y2;

    for (int y = y1; y <= y2; y++) 
    {
        for (int x = x1; x <= x2; x++) 
        {
            bool chroma_color = (px_map[(hor_res >> 3) * y  + (x >> 3)] & 1 << (7 - x % 8));

            uint8_t *buf = oled_buffer + hor_res * (y >> 3) + (x);

            if(chroma_color) 
                (*buf) &= ~(1 << (y % 8));
            
            else 
                (*buf) |= (1 << (y % 8));
        }
    }
   
    esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x2 + 1, y2 + 1, oled_buffer);
}

static void increase_lvgl_tick(void *arg)
{
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

static void ui_create(lv_display_t *display)
{
    lv_obj_t *scr = lv_display_get_screen_active(display);

    label_setpoint = lv_label_create(scr);
    lv_label_set_long_mode(label_setpoint, LV_LABEL_LONG_WRAP);
    lv_obj_align(label_setpoint, LV_ALIGN_TOP_LEFT, 10, 10);

    label_temperature = lv_label_create(scr);
    lv_label_set_long_mode(label_temperature, LV_LABEL_LONG_WRAP);
    lv_obj_align(label_temperature, LV_ALIGN_TOP_MID, 0, 10);

    label_speed = lv_label_create(scr);
    lv_label_set_long_mode(label_speed, LV_LABEL_LONG_WRAP);
    lv_obj_align(label_speed, LV_ALIGN_TOP_RIGHT, -10, 10);
}

static void ui_update(sensor_data_t sensor_data)
{
    _lock_acquire(&lvgl_api_lock);

    char setpoint_buf[32];
    char temperature_buf[32];
    char fan_buf[32];

    snprintf(setpoint_buf, sizeof(setpoint_buf), "SP: %.1f°C", sensor_data.setpoint);
    snprintf(temperature_buf, sizeof(temperature_buf), "TMP: %.1f°C", sensor_data.temperature);
    snprintf(fan_buf, sizeof(fan_buf), "FAN: %.0f%%", current_cooler_percent);

    lv_label_set_text(label_setpoint, setpoint_buf);
    lv_label_set_text(label_temperature, temperature_buf);
    lv_label_set_text(label_speed, fan_buf);

    _lock_release(&lvgl_api_lock);
}

static float clampf(float value, float min, float max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static void update_setpoint(float new_setpoint)
{
    farm_controller.setpoint = clampf(new_setpoint, MIN_TEMPERATURE, MAX_TEMPERATURE);
    farm_data.setpoint = farm_controller.setpoint;
    ESP_LOGI(TAG_FARM, "Setpoint definido para %.1f°C", farm_controller.setpoint);
}

TickType_t delay_ms(int milisseconds) 
{
    return (milisseconds / portTICK_PERIOD_MS);
}

static void timer_task(void* arg)
{
    cclock_t Clock;
    
    timer_queue = xQueueCreate(10, sizeof(Clock));
    
    if(!timer_queue) 
    {
        ESP_LOGE(TAG_FARM, "ERRO: Não foi possível criar a fila corretamente.");
        return;
    }
    
    gptimer_handle_t gptimer = NULL;

    gptimer_config_t timer_config = 
    {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = TIMER_RESOLUTION_HZ,
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
        .alarm_count = UPDATE_RATE_TICKS, 
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));
    ESP_ERROR_CHECK(gptimer_start(gptimer));

    while(1)
    {
        if(xQueueReceive(timer_queue, &Clock, pdMS_TO_TICKS(1500)))   
        {
            xSemaphoreGive(semaphore_adc);
        }
    }
}

static void pwm_task(void* arg)
{
    ledc_timer_config_t ledc_timer = 
    {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .duty_resolution  = LEDC_TIMER_13_BIT,
        .timer_num        = LEDC_TIMER_0,
        .freq_hz          = 5000,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel_0 = 
    {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = GPIO_OUTPUT_IO_17,
        .duty           = 0,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_0));

    ledc_channel_config_t ledc_channel_1 = 
    {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_1,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = GPIO_OUTPUT_IO_26,
        .duty           = 0,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_1));

    duty_t farm_duty;

    pwm_queue = xQueueCreate(10, sizeof(duty_t));

    if(pwm_queue == NULL) 
    {
        ESP_LOGE(TAG_FARM, "ERRO: Não foi possível criar a fila de PWM.");
        return;
    }

    while(1)
    {
        if(xQueueReceive(pwm_queue, &farm_duty, pdMS_TO_TICKS(1500)))
        {
            ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, farm_duty.heater));
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0)); 
            ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, farm_duty.cooler));
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1));
       }
    }
}

static void adc_task(void* arg)
{
    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t init_config = 
    {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    adc_oneshot_chan_cfg_t config = 
    {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_3, &config));

    adc_cali_handle_t adc_cali_handle = NULL;

    adc_cali_line_fitting_config_t cali_config = 
    {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
                
    ESP_ERROR_CHECK(adc_cali_create_scheme_line_fitting(&cali_config, &adc_cali_handle));
    
    int adc_raw, adc_cali;

    semaphore_adc = xSemaphoreCreateBinary();

    while(1)
    {
        if(xSemaphoreTake(semaphore_adc, portMAX_DELAY) == pdTRUE)
        {   
            ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL_3, &adc_raw));
  
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc_cali_handle, adc_raw, &adc_cali));

            float measured_temp = adc_cali / 10.0f; // LM35: 10 mV / °C
            ESP_LOGI(TAG_FARM, "adc_raw %d mV - adc_cali %d mV - temp %.1f°C", adc_raw, adc_cali, measured_temp);
            
            farm_data.temperature = measured_temp;
            farm_data.setpoint = farm_controller.setpoint;

            xQueueSend(controller_queue, &measured_temp, pdMS_TO_TICKS(10));
            
            ui_update(farm_data);
        }
   }
}

static void lvgl_port_task(void *arg)
{
    uint32_t time_till_next_ms = 0;

    while (1) 
    {
        _lock_acquire(&lvgl_api_lock);
        time_till_next_ms = lv_timer_handler();
        _lock_release(&lvgl_api_lock);
   
        time_till_next_ms = MAX(time_till_next_ms, LVGL_TASK_MIN_DELAY_MS);
        time_till_next_ms = MIN(time_till_next_ms, LVGL_TASK_MAX_DELAY_MS);

        usleep(1000 * time_till_next_ms);
    }
}

static void display_task(void *arg)
{
    i2c_master_bus_handle_t i2c_handle = NULL;
    i2c_master_bus_config_t i2c_config = 
    {
        .clk_source                     = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt              = 7,
        .i2c_port                       = I2C_BUS_PORT,
        .sda_io_num                     = PIN_SDA,
        .scl_io_num                     = PIN_SCL,
        .flags.enable_internal_pullup   = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_config, &i2c_handle));

    esp_lcd_panel_io_handle_t lcd_handle = NULL;
    esp_lcd_panel_io_i2c_config_t lcd_config = 
    {
        .dev_addr               = I2C_HW_ADDR,
        .scl_speed_hz           = LCD_PIXEL_CLOCK_HZ,
        .control_phase_bytes    = 1,               
        .lcd_cmd_bits           = LCD_CMD_BITS,   
        .lcd_param_bits         = LCD_CMD_BITS, 
        .dc_bit_offset          = 6,                     
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_handle, &lcd_config, &lcd_handle));
 
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = 
    {
        .bits_per_pixel = 1,
        .reset_gpio_num = PIN_NUM_RST,
    };

    esp_lcd_panel_ssd1306_config_t ssd1306_config = 
    {
        .height = LCD_V_RES,
    };
    panel_config.vendor_config = &ssd1306_config;

    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(lcd_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    ESP_LOGI(TAG_FARM, "Inicializando lib LVGL...");
    lv_init();

    /* CRIAÇÃO DO DISPLAY */
    lv_display_t *display = lv_display_create(LCD_H_RES, LCD_V_RES);

    lv_display_set_user_data(display, panel_handle);

    void *buf = NULL;
    
    size_t draw_buffer_sz = LCD_H_RES * LCD_V_RES / 8 + LVGL_PALETTE_SIZE;
    buf = heap_caps_calloc(1, draw_buffer_sz, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    assert(buf);

    lv_display_set_color_format(display, LV_COLOR_FORMAT_I1);
    lv_display_set_buffers(display, buf, NULL, draw_buffer_sz, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, lvgl_flush_cb);

    const esp_lcd_panel_io_callbacks_t callback = 
    {
        .on_color_trans_done = notify_lvgl_flush_ready,
    };
    esp_lcd_panel_io_register_event_callbacks(lcd_handle, &callback, display);

    const esp_timer_create_args_t lvgl_tick_timer_args =
    {
        .callback = &increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;

    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));

    xTaskCreate(lvgl_port_task, "lvgl_port_task", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, NULL);

    _lock_acquire(&lvgl_api_lock);
    ui_create(display);
    _lock_release(&lvgl_api_lock);

    while (1) 
    {
        vTaskDelay(delay_ms(250));
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t  event  = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch((esp_mqtt_event_id_t)event_id) 
    {
        case MQTT_EVENT_CONNECTED:

            ESP_LOGI(TAG_FARM, "ESP se conectou com sucesso");   
            esp_mqtt_client_subscribe(client, "/topic/farm_setpoint", 0);

            break;

        case MQTT_EVENT_DISCONNECTED:
            break;

        case MQTT_EVENT_SUBSCRIBED:

            ESP_LOGI(TAG_FARM, "ESP se inscreveu num tópico");   

            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            break;

        case MQTT_EVENT_PUBLISHED:
            break;

        case MQTT_EVENT_DATA:
        {
            const char *topic = "/topic/farm_setpoint";
            size_t topic_len = strlen(topic);

            if ((size_t)event->topic_len == topic_len && strncmp(event->topic, topic, topic_len) == 0)
            {
                char value_buf[32] = {0};
                size_t len = event->data_len;

                if(len >= sizeof(value_buf))
                    len = sizeof(value_buf) - 1;
                
                memcpy(value_buf, event->data, len);
                value_buf[len] = '\0';

                float new_setpoint = strtof(value_buf, NULL);
                if (new_setpoint >= MIN_TEMPERATURE && new_setpoint <= MAX_TEMPERATURE)
                    update_setpoint(new_setpoint);
                else
                    ESP_LOGW(TAG_FARM, "Setpoint MQTT fora do intervalo: %s", value_buf);
            }
            break;
        }

        case MQTT_EVENT_ERROR:

            ESP_LOGE(TAG_FARM, "ERRO: Um erro aconteceu!");

            if(event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) 
            {
                ESP_LOGE(TAG_FARM, "Reportado por esp_tls_last_esp_err: %d", event->error_handle->esp_tls_last_esp_err);
                ESP_LOGE(TAG_FARM, "Reportado por esp_tls_stack_err: %d", event->error_handle->esp_tls_stack_err);
                ESP_LOGE(TAG_FARM, "Reportado por esp_transport_sock_errno: %d", event->error_handle->esp_transport_sock_errno);
                ESP_LOGE(TAG_FARM, "Erro string: (%s)", strerror(event->error_handle->esp_transport_sock_errno));
            }

            break;
        
        default:

            ESP_LOGI(TAG_FARM, "???: Um evento desconhecido foi executado! event_id: %d", event->event_id);
        
        break;
    }
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = 
    {
        .broker.address.uri = "mqtt://g1device:g1device@node02.myqtthub.com:1883",
        .credentials.client_id = "g1device",
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);

    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

static void controller_task(void* arg)
{
    controller_queue = xQueueCreate(10, sizeof(float));

    if(controller_queue == NULL) 
    {
        ESP_LOGE(TAG_FARM, "ERRO: Não foi possível criar a fila do controlador.");
        return;
    }

    update_setpoint(farm_controller.setpoint);

    pid_ctrl_config_f_t pid_config = 
    {
        .init_param = 
        {
            .kp = 5.0f,
            .ki = 0.3f,
            .kd = 0.0f,
            .max_output = PID_OUTPUT_MAX,
            .min_output = PID_OUTPUT_MIN,
            .max_integral = 50.0f,
            .min_integral = -50.0f,
            .cal_type = PID_CAL_TYPE_POSITIONAL,
        },
    };
    ESP_ERROR_CHECK(pid_new_control_block_f(&pid_config, &farm_controller.pid_handle));

    float measured_temp = 0.0f;
    duty_t next_duty = {0};

    while(1) 
    {
        if(xQueueReceive(controller_queue, &measured_temp, pdMS_TO_TICKS(UPDATE_RATE_MS * 2)))
        {
            farm_controller.temperature = measured_temp;
            float error = farm_controller.setpoint - measured_temp;
            float pid_output = 0.0f;

            ESP_ERROR_CHECK(pid_compute(farm_controller.pid_handle, error, &pid_output));
            float heater_pct = clampf(pid_output, PID_OUTPUT_MIN, PID_OUTPUT_MAX);
            float cooler_pct = 0.0f;

            if(measured_temp > farm_controller.setpoint + COOLER_DEADBAND_C) 
            {
                cooler_pct = clampf((measured_temp - farm_controller.setpoint) * COOLER_SCALE_PERCENT, PID_OUTPUT_MIN, PID_OUTPUT_MAX);
            }

            current_heater_percent = heater_pct;
            current_cooler_percent = cooler_pct;

            next_duty.heater = (uint16_t)((heater_pct / 100.0f) * PWM_MAX_DUTY + 0.5f);
            next_duty.cooler = (uint16_t)((cooler_pct / 100.0f) * PWM_MAX_DUTY + 0.5f);

            xQueueSend(pwm_queue, &next_duty, pdMS_TO_TICKS(10));
        }
    }
}

void app_main(void)
{
    xTaskCreate(timer_task, "timer_task", 2048, NULL, 1, NULL);
    
    xTaskCreate(controller_task, "controller_task", 2048, NULL, 2, NULL);

    xTaskCreate(pwm_task, "pwm_task", 2048, NULL, 1, NULL);

    xTaskCreate(adc_task, "adc_task", 4096, NULL, 1, NULL);

    xTaskCreate(display_task, "display_task", 4096, NULL, 1, NULL);
    
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(example_connect());

    mqtt_app_start();

    while(1)
    {
        vTaskDelay(delay_ms(15000));
    }
}
