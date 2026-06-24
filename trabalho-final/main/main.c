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
//#include "driver/i2c_master.h"
#include "soc/soc_caps.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_event.h"
#include "esp_netif.h"
//#include "lvgl.h"
#include "nvs_flash.h"
//#include "protocol_examples_common.h"
//#include "mqtt_client.h"

// static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t io_panel, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
// {
//     lv_display_t *disp = (lv_display_t *)user_ctx;
//     lv_display_flush_ready(disp);
//     return false;
// }

// static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
// {
//     esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(disp);

//     px_map += LVGL_PALETTE_SIZE;

//     uint16_t hor_res = lv_display_get_physical_horizontal_resolution(disp);
//     int x1 = area->x1;
//     int x2 = area->x2;
//     int y1 = area->y1;
//     int y2 = area->y2;

//     for (int y = y1; y <= y2; y++) 
//     {
//         for (int x = x1; x <= x2; x++) 
//         {
//             bool chroma_color = (px_map[(hor_res >> 3) * y  + (x >> 3)] & 1 << (7 - x % 8));

//             uint8_t *buf = oled_buffer + hor_res * (y >> 3) + (x);

//             if (chroma_color) 
//             {
//                 (*buf) &= ~(1 << (y % 8));
//             } 

//             else 
//             {
//                 (*buf) |= (1 << (y % 8));
//             }
//         }
//     }
   
//     esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x2 + 1, y2 + 1, oled_buffer);
// }

// static void increase_lvgl_tick(void *arg)
// {
//     lv_tick_inc(LVGL_TICK_PERIOD_MS);
// }

// static void ui_create(lv_display_t *display)
// {
//     lv_obj_t *scr = lv_display_get_screen_active(display);

//     label_clock = lv_label_create(scr);
//     lv_obj_align(label_clock, LV_ALIGN_TOP_MID, 0, 10);

//     label_adc = lv_label_create(scr);
//     lv_obj_align(label_adc, LV_ALIGN_BOTTOM_MID, 0, 0);
// }

// static void ui_update(cclock_t Clock, int voltage)
// {
//     _lock_acquire(&lvgl_api_lock);

//     //lv_label_set_text(label_clock, clock_text);
//     //(label_adc, adc_text);

//     _lock_release(&lvgl_api_lock);
// }

// static void timer_task(void* arg)
// {
//     cclock_t Clock;
    
//     timer_queue = xQueueCreate(10, sizeof(Clock));
    
//     if(!timer_queue) 
//     {
//         ESP_LOGE(TAG_4, "ERRO: Não foi possível criar a fila corretamente.");
//         return;
//     }
    
//     gptimer_handle_t gptimer = NULL;

//     gptimer_config_t timer_config = 
//     {
//         .clk_src = GPTIMER_CLK_SRC_DEFAULT,
//         .direction = GPTIMER_COUNT_UP,
//         .resolution_hz = 1000000, // 1MHz
//     };
//     ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

//     gptimer_event_callbacks_t callback = 
//     {
//         .on_alarm = OnMilisUpdate,
//     };
//     ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &callback, timer_queue));

//     ESP_ERROR_CHECK(gptimer_enable(gptimer));
    
//     gptimer_alarm_config_t alarm_config = 
//     {
//         .alarm_count = 100000, 
//     };
//     ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));
//     ESP_ERROR_CHECK(gptimer_start(gptimer));

//     adc_cali_handle_t adc_cali_handle = NULL;

//     adc_cali_line_fitting_config_t cali_config = 
//     {
//         .unit_id = ADC_UNIT_1,
//         .atten = ADC_ATTEN_DB_12,
//         .bitwidth = ADC_BITWIDTH_DEFAULT,
//     };
                
//     ESP_ERROR_CHECK(adc_cali_create_scheme_line_fitting(&cali_config, &adc_cali_handle));
    
//     int adc_raw;
//     int adc_cali;

//     while(1)
//     {
//         if(xQueueReceive(timer_queue, &Clock, pdMS_TO_TICKS(1500)))   
//         {
//             Clock.milis    = GetMilisFromMHz(Clock.alarm_value);
//             Clock.days     = Clock.milis / (1000 * 60 * 60 * 24);
//             Clock.hours    = Clock.milis / (1000 * 60 * 60) % 24;
//             Clock.minutes  = Clock.milis / (1000 * 60) % 60;
//             Clock.seconds  = Clock.milis / (1000) % 60;

//             ESP_LOGI(TAG_5, "Dias: %02llu | %02llu:%02llu:%02llu", 
//                     Clock.days, Clock.hours, Clock.minutes, Clock.seconds);
            
//             xSemaphoreGive(semaphore_pwm);
//             xSemaphoreGive(semaphore_adc);

//             if(xQueueReceive(adc_queue, &adc_raw, pdMS_TO_TICKS(10)))
//             {
//                 if(!(Clock.alarm_value % 1000000))
//                 {
//                     ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc_cali_handle, adc_raw, &adc_cali));
//                     ESP_LOGI(TAG_7, "adc_raw %d mV - adc_cali %d mV", adc_raw, adc_cali);

//                     ui_update(Clock, adc_cali);
//                 }
//             }
//         }
//         else  
//             ESP_LOGW(TAG_4, "Aviso: Contagem perdida!");
//     }
// }

// static void pwm_task(void* arg)
// {
//     PWM_elements_t PWM;
//     uint32_t io_num;

//     ledc_timer_config_t ledc_timer = 
//     {
//         .speed_mode       = LEDC_LOW_SPEED_MODE,
//         .duty_resolution  = LEDC_TIMER_13_BIT,
//         .timer_num        = LEDC_TIMER_0,
//         .freq_hz          = 5000,
//         .clk_cfg          = LEDC_AUTO_CLK
//     };
//     ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

//     ledc_channel_config_t ledc_channel_0 = 
//     {
//         .speed_mode     = LEDC_LOW_SPEED_MODE,
//         .channel        = LEDC_CHANNEL_0,
//         .timer_sel      = LEDC_TIMER_0,
//         .intr_type      = LEDC_INTR_DISABLE,
//         .gpio_num       = GPIO_OUTPUT_IO_16,
//         .duty           = 0,
//         .hpoint         = 0
//     };
//     ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_0));

//     ledc_channel_config_t ledc_channel_1 = {
//         .speed_mode     = LEDC_LOW_SPEED_MODE,
//         .channel        = LEDC_CHANNEL_1,
//         .timer_sel      = LEDC_TIMER_0,
//         .intr_type      = LEDC_INTR_DISABLE,
//         .gpio_num       = GPIO_OUTPUT_IO_33,
//         .duty           = 0,
//         .hpoint         = 0
//     };
//     ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_1));

//     ledc_channel_config_t ledc_channel_2 = 
//     {
//         .speed_mode     = LEDC_LOW_SPEED_MODE,
//         .channel        = LEDC_CHANNEL_2,
//         .timer_sel      = LEDC_TIMER_0,
//         .intr_type      = LEDC_INTR_DISABLE,
//         .gpio_num       = GPIO_OUTPUT_IO_17,
//         .duty           = 0,
//         .hpoint         = 0
//     };
//     ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_2));

//     ledc_channel_config_t ledc_channel_3 = 
//     {
//         .speed_mode     = LEDC_LOW_SPEED_MODE,
//         .channel        = LEDC_CHANNEL_3,
//         .timer_sel      = LEDC_TIMER_0,
//         .intr_type      = LEDC_INTR_DISABLE,
//         .gpio_num       = GPIO_OUTPUT_IO_26, //vermelho
//         .duty           = 0,
//         .hpoint         = 0
//     };
//     ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_3));
    
//     pwm_queue = xQueueCreate(10, sizeof(uint32_t));

//     semaphore_pwm = xSemaphoreCreateBinary();

//     PWM.mode = false;
//     PWM.duty.pin1 = 0;
//     PWM.duty.pin2 = 0;
//     PWM.duty.pin3 = 0;
    
//     while(1)
//     {
//         if(xQueueReceive(pwm_queue, &io_num, pdMS_TO_TICKS(10)))
//         {
//             vTaskDelay(delay_ms(50));
            
//             if(gpio_get_level(io_num)) continue;
            
//             switch(io_num)
//             {
//                 case ( GPIO_INPUT_IO_21 ):  
//                     PWM.mode = true;
//                 break;

//                 case ( GPIO_INPUT_IO_22 ):    
//                     PWM.mode = false;
//                     ESP_LOGW(TAG_6, "Modo: Manual | Duty: %d", PWM.duty.pin1);
//                     UpdatePWN(PWM.duty);
//                 break;
            
//                 case ( GPIO_INPUT_IO_23 ):
//                     if(!PWM.mode)
//                     {
//                         PWM.duty.pin1 = (PWM.duty.pin1 >= 8192) ? 0 : PWM.duty.pin1 + PWM_AUTO_INCREMENT;
                        
//                         ESP_LOGW(TAG_6, "Modo: Manual | Duty: %d", PWM.duty.pin1);
//                         UpdatePWN(PWM.duty);
//                     }
//                 break;
//             }
//         }

//         if(xSemaphoreTake(semaphore_pwm, portMAX_DELAY) == pdTRUE)
//         {            
//             if(PWM.mode)
//             {
//                 UpdatePWN(PWM.duty);
            
//                 PWM.duty.pin1 = (PWM.duty.pin1 >= 8192) ? 0 : PWM.duty.pin1 + PWM_AUTO_INCREMENT; 
//                 ESP_LOGI(TAG_6, "Modo: Automático | Duty: %d", PWM.duty.pin1);
//             }
//         }
//     }
// }

// static void adc_task(void* arg)
// {
//     adc_oneshot_unit_handle_t adc_handle;
//     adc_oneshot_unit_init_cfg_t init_config = 
//     {
//         .unit_id = ADC_UNIT_1,
//     };
    
//     ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

//     adc_oneshot_chan_cfg_t config = 
//     {
//         .bitwidth = ADC_BITWIDTH_DEFAULT,
//         .atten = ADC_ATTEN_DB_12,
//     };
//     ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_3, &config));

//     int adc_raw;

//     adc_queue = xQueueCreate(10, sizeof(int));

//     semaphore_adc = xSemaphoreCreateBinary();

//     while(1)
//     {
//         if(xSemaphoreTake(semaphore_adc, portMAX_DELAY) == pdTRUE)
//         {   
//             ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL_3, &adc_raw));
     
//             xQueueSendFromISR(adc_queue, &adc_raw, NULL);
//         }
//    }
    
// }

// static void lvgl_port_task(void *arg)
// {
//     uint32_t time_till_next_ms = 0;

//     while (1) 
//     {
//         _lock_acquire(&lvgl_api_lock);
//         time_till_next_ms = lv_timer_handler();
//         _lock_release(&lvgl_api_lock);
   
//         time_till_next_ms = MAX(time_till_next_ms, LVGL_TASK_MIN_DELAY_MS);
//         time_till_next_ms = MIN(time_till_next_ms, LVGL_TASK_MAX_DELAY_MS);

//         usleep(1000 * time_till_next_ms);
//     }
// }

// static void display_task(void *arg)
// {
//     i2c_master_bus_handle_t i2c_handle = NULL;
//     i2c_master_bus_config_t i2c_config = 
//     {
//         .clk_source                     = I2C_CLK_SRC_DEFAULT,
//         .glitch_ignore_cnt              = 7,
//         .i2c_port                       = I2C_BUS_PORT,
//         .sda_io_num                     = PIN_SDA,
//         .scl_io_num                     = PIN_SCL,
//         .flags.enable_internal_pullup   = true,
//     };
//     ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_config, &i2c_handle));

//     esp_lcd_panel_io_handle_t lcd_handle = NULL;
//     esp_lcd_panel_io_i2c_config_t lcd_config = 
//     {
//         .dev_addr               = I2C_HW_ADDR,
//         .scl_speed_hz           = LCD_PIXEL_CLOCK_HZ,
//         .control_phase_bytes    = 1,               
//         .lcd_cmd_bits           = LCD_CMD_BITS,   
//         .lcd_param_bits         = LCD_CMD_BITS, 
//         .dc_bit_offset          = 6,                     
//     };
//     ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_handle, &lcd_config, &lcd_handle));
 
//     esp_lcd_panel_handle_t panel_handle = NULL;
//     esp_lcd_panel_dev_config_t panel_config = 
//     {
//         .bits_per_pixel = 1,
//         .reset_gpio_num = PIN_NUM_RST,
//     };

//     esp_lcd_panel_ssd1306_config_t ssd1306_config = 
//     {
//         .height = LCD_V_RES,
//     };
//     panel_config.vendor_config = &ssd1306_config;

//     ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(lcd_handle, &panel_config, &panel_handle));

//     ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
//     ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
//     ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

//     ESP_LOGI(TAG_8, "Inicializando lib LVGL...");
//     lv_init();

//     /* CRIAÇÃO DO DISPLAY */
//     lv_display_t *display = lv_display_create(LCD_H_RES, LCD_V_RES);

//     lv_display_set_user_data(display, panel_handle);

//     void *buf = NULL;
    
//     size_t draw_buffer_sz = LCD_H_RES * LCD_V_RES / 8 + LVGL_PALETTE_SIZE;
//     buf = heap_caps_calloc(1, draw_buffer_sz, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
//     assert(buf);

//     lv_display_set_color_format(display, LV_COLOR_FORMAT_I1);
//     lv_display_set_buffers(display, buf, NULL, draw_buffer_sz, LV_DISPLAY_RENDER_MODE_FULL);
//     lv_display_set_flush_cb(display, lvgl_flush_cb);

//     const esp_lcd_panel_io_callbacks_t callback = 
//     {
//         .on_color_trans_done = notify_lvgl_flush_ready,
//     };
//     esp_lcd_panel_io_register_event_callbacks(lcd_handle, &callback, display);

//     const esp_timer_create_args_t lvgl_tick_timer_args =
//     {
//         .callback = &increase_lvgl_tick,
//         .name = "lvgl_tick"
//     };
//     esp_timer_handle_t lvgl_tick_timer = NULL;

//     ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
//     ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));

//     xTaskCreate(lvgl_port_task, "lvgl_port_task", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, NULL);

//     _lock_acquire(&lvgl_api_lock);
//     ui_create(display);
//     _lock_release(&lvgl_api_lock);

//     while (1) 
//     {
//         vTaskDelay(delay_ms(250));
//     }
// }

TickType_t delay_ms(int milisseconds) 
{
    return (milisseconds / portTICK_PERIOD_MS);
}
   
#define GPIO_OUTPUT_PIN_SEL ( 1ULL << 17 )

void app_main(void)
{
    gpio_config_t output_config = 
    {
        .pin_bit_mask = GPIO_OUTPUT_PIN_SEL,
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };

    if(gpio_config(&output_config) != ESP_OK)
        ESP_LOGE("[ ]", "ERRO: Não foi possível configurar gpio's!\n");    
    else
        ESP_LOGI("[ ]", "GPIO INPUT: Configuração realizada com sucesso.");

    while(1)
    {
        gpio_set_level(17, gpio_get_level(17) ? 0 : 1);
        vTaskDelay(delay_ms(15000));
    }
}

