/*
* HomeKit interface for PWM control 12v IKEA Dioder leds.
* 
*/
#include <stdio.h>
#include <stdlib.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <math.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include "wifi.h"

#include "converters.h"
#include "multipwm.h"

#define LED_INBUILT_GPIO 2
#define WHITE_GPIO       4
#define RED_GPIO         13
#define GREEN_GPIO       14
#define BLUE_GPIO        12

#define PWM_MAX 1024

// pwm
pwm_info_t pwm_info;

// Home Kit variables
// color
bool hkc_on = true;
float hkc_hue = 0;
float hkc_saturation = 0;
int hkc_brightness = 100;
// white
bool hkw_on = true;
int hkw_brightness = 100;

static void wifi_init() {
    struct sdk_station_config wifi_config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASSWORD,
    };

    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&wifi_config);
    sdk_wifi_station_connect();
}

void update_pwm() {
    int white_pin_value = 0;

    if (hkw_on) {
        white_pin_value = map(hkw_brightness, 0, 100, 0, UINT16_MAX);
    }

    multipwm_set_duty(&pwm_info, 0, white_pin_value);
    
    int red_pin_value = 0;
    int green_pin_value = 0;
    int blue_pin_value = 0;
    if (hkc_on) {
        int r, g, b;
        float dim = hkc_brightness / 100.0f;

        hs2rgb(hkc_hue, hkc_saturation / 100.0f, &r, &g, &b);
        //printf("hue:%f sat:%f rgb:%02x%02x%02x\n", hk_hue, hk_saturation, color.red, color.green, color.blue);

        red_pin_value = map(r * dim, 0, 255, 0, UINT16_MAX);
        green_pin_value = map(g * dim, 0, 255, 0, UINT16_MAX);
        blue_pin_value = map(b * dim, 0, 255, 0, UINT16_MAX);
    }
    
    multipwm_set_duty(&pwm_info, 1, red_pin_value);
    multipwm_set_duty(&pwm_info, 2, green_pin_value);
    multipwm_set_duty(&pwm_info, 3, blue_pin_value);
}

void init_pwm() {
    pwm_info.channels = 4;

    multipwm_init(&pwm_info);
    multipwm_set_pin(&pwm_info, 0, WHITE_GPIO);
    multipwm_set_pin(&pwm_info, 1, RED_GPIO);
    multipwm_set_pin(&pwm_info, 2, GREEN_GPIO);
    multipwm_set_pin(&pwm_info, 3, BLUE_GPIO);

    multipwm_start(&pwm_info);
    
    update_pwm();
}

void identify_task(void *_args) {
    // initialise the onboard led as a secondary indicator (handy for testing)
    gpio_enable(LED_INBUILT_GPIO, GPIO_OUTPUT);

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            gpio_write(LED_INBUILT_GPIO, 0);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            gpio_write(LED_INBUILT_GPIO, 1);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }

    gpio_write(LED_INBUILT_GPIO, 1);

    vTaskDelete(NULL);
}

void identify(homekit_value_t _value) {
    xTaskCreate(identify_task, "LED identify", 128, NULL, 2, NULL);
}

homekit_value_t color_on_get() {
    return HOMEKIT_BOOL(hkc_on);
}

void color_on_set(homekit_value_t value) {
    hkc_on = value.bool_value;

    update_pwm();
}

homekit_value_t color_brightness_get() {
    return HOMEKIT_INT(hkc_brightness);
}

void color_brightness_set(homekit_value_t value) {
    hkc_brightness = value.int_value;

    update_pwm();
}

homekit_value_t color_hue_get() {
    return HOMEKIT_FLOAT(hkc_hue);
}

void color_hue_set(homekit_value_t value) {
    hkc_hue = value.float_value;

    update_pwm();
}

homekit_value_t color_saturation_get() {
    return HOMEKIT_FLOAT(hkc_saturation);
}

void color_saturation_set(homekit_value_t value) {
    hkc_saturation = value.float_value;
    
    update_pwm();
}

homekit_value_t white_on_get() {
    return HOMEKIT_BOOL(hkw_on);
}

void white_on_set(homekit_value_t value) {
    hkw_on = value.bool_value;

    update_pwm();
}

homekit_value_t white_brightness_get() {
    return HOMEKIT_INT(hkw_brightness);
}

void white_brightness_set(homekit_value_t value) {
    hkw_brightness = value.int_value;

    update_pwm();
}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Kamaji");

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(
        .id = 1,
        .category = homekit_accessory_category_lightbulb,
        .services = (homekit_service_t*[]) {
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]) {
            &name,
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Generic"),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "3f620465b61d"),
            HOMEKIT_CHARACTERISTIC(MODEL, "DioderDator"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, identify),
            NULL
        }),
        HOMEKIT_SERVICE(LIGHTBULB, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Kamaji-Color"),
            HOMEKIT_CHARACTERISTIC(
                ON, true,
            .getter = color_on_get,
            .setter = color_on_set
                ),
            HOMEKIT_CHARACTERISTIC(
                BRIGHTNESS, 100,
            .getter = color_brightness_get,
            .setter = color_brightness_set
                ),
            HOMEKIT_CHARACTERISTIC(
                HUE, 0,
            .getter = color_hue_get,
            .setter = color_hue_set
                ),
            HOMEKIT_CHARACTERISTIC(
                SATURATION, 0,
            .getter = color_saturation_get,
            .setter = color_saturation_set
                ),
            NULL
        }),
        HOMEKIT_SERVICE(LIGHTBULB, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Kamaji-White"),
            HOMEKIT_CHARACTERISTIC(
                ON, true,
            .getter = white_on_get,
            .setter = white_on_set
                ),
            HOMEKIT_CHARACTERISTIC(
                BRIGHTNESS, 100,
            .getter = white_brightness_get,
            .setter = white_brightness_set
                ),
            NULL
        }),
        NULL
    }),
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111"
};

void user_init(void) {
    // This example shows how to use same firmware for multiple similar accessories
    // without name conflicts. It uses the last 3 bytes of accessory's MAC address as
    // accessory name suffix.
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);
    int name_len = snprintf(NULL, 0, "Kamaji-%02X%02X%02X", macaddr[3], macaddr[4], macaddr[5]);
    char *name_value = malloc(name_len + 1);
    snprintf(name_value, name_len + 1, "Kamaji-%02X%02X%02X", macaddr[3], macaddr[4], macaddr[5]);
    name.value = HOMEKIT_STRING(name_value);

    wifi_init();

    homekit_server_init(&config);
    
    init_pwm();

    identify(HOMEKIT_INT(1));
}
