extern "C" {
#include "bsp/device.h"
#include "bsp/display.h"
#include "bsp/input.h"
#include "bsp/audio.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "esp_pthread.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdcard.h"
}
#include <SDL3/SDL.h>
#include <pthread.h>

extern "C" int main(int argc, char **argv);

static QueueHandle_t input_queue;
static const char *TAG = "openjazz-main";
static float hardware_volume = 45.0f;

static void *openjazz_thread(void *) {
    char arg0[] = "OpenJazz";
    char arg1[] = "/sd/openjazz";
    char *argv[] = {arg0, arg1, nullptr};
    return reinterpret_cast<void *>(static_cast<intptr_t>(main(2, argv)));
}

static SDL_Keycode nav_key(bsp_input_navigation_key_t key) {
    switch (key) {
        case BSP_INPUT_NAVIGATION_KEY_UP: return SDLK_UP;
        case BSP_INPUT_NAVIGATION_KEY_DOWN: return SDLK_DOWN;
        case BSP_INPUT_NAVIGATION_KEY_LEFT: return SDLK_LEFT;
        case BSP_INPUT_NAVIGATION_KEY_RIGHT: return SDLK_RIGHT;
        case BSP_INPUT_NAVIGATION_KEY_SELECT: return SDLK_RETURN;
        case BSP_INPUT_NAVIGATION_KEY_START: return SDLK_RETURN;
        case BSP_INPUT_NAVIGATION_KEY_ESC: return SDLK_ESCAPE;
        case BSP_INPUT_NAVIGATION_KEY_MENU: return SDLK_ESCAPE;
        case BSP_INPUT_NAVIGATION_KEY_BACKSPACE: return SDLK_ESCAPE;
        case BSP_INPUT_NAVIGATION_KEY_RETURN: return SDLK_RETURN;
        case BSP_INPUT_NAVIGATION_KEY_SPACE_L:
        case BSP_INPUT_NAVIGATION_KEY_SPACE_M:
        case BSP_INPUT_NAVIGATION_KEY_SPACE_R: return SDLK_SPACE;
        default: return SDLK_UNKNOWN;
    }
}

static SDL_Keycode scancode_key(bsp_input_scancode_t scancode) {
    switch (scancode & ~BSP_INPUT_SCANCODE_RELEASE_MODIFIER) {
        case BSP_INPUT_SCANCODE_ESC: return SDLK_ESCAPE;
        case BSP_INPUT_SCANCODE_1: return SDLK_1;
        case BSP_INPUT_SCANCODE_2: return SDLK_2;
        case BSP_INPUT_SCANCODE_3: return SDLK_3;
        case BSP_INPUT_SCANCODE_4: return SDLK_4;
        case BSP_INPUT_SCANCODE_5: return SDLK_5;
        case BSP_INPUT_SCANCODE_ESCAPED_MENU: return SDLK_ESCAPE;
        case BSP_INPUT_SCANCODE_ESCAPED_BACK: return SDLK_ESCAPE;
        case BSP_INPUT_SCANCODE_BACKSPACE: return SDLK_ESCAPE;
        case BSP_INPUT_SCANCODE_ENTER: return SDLK_RETURN;
        case BSP_INPUT_SCANCODE_LEFTCTRL:
        case BSP_INPUT_SCANCODE_ESCAPED_RCTRL: return SDLK_RCTRL;
        case BSP_INPUT_SCANCODE_LEFTALT:
        case BSP_INPUT_SCANCODE_ESCAPED_RALT: return SDLK_LALT;
        case BSP_INPUT_SCANCODE_LEFTSHIFT:
        case BSP_INPUT_SCANCODE_RIGHTSHIFT: return SDLK_LSHIFT;
        case BSP_INPUT_SCANCODE_ESCAPED_KPENTER: return SDLK_RETURN;
        case BSP_INPUT_SCANCODE_SPACE: return SDLK_SPACE;
        case BSP_INPUT_SCANCODE_Q: return SDLK_Q;
        case BSP_INPUT_SCANCODE_W: return SDLK_W;
        case BSP_INPUT_SCANCODE_E: return SDLK_E;
        case BSP_INPUT_SCANCODE_R: return SDLK_R;
        case BSP_INPUT_SCANCODE_T: return SDLK_T;
        case BSP_INPUT_SCANCODE_U: return SDLK_U;
        case BSP_INPUT_SCANCODE_I: return SDLK_I;
        case BSP_INPUT_SCANCODE_O: return SDLK_O;
        case BSP_INPUT_SCANCODE_P: return SDLK_P;
        case BSP_INPUT_SCANCODE_A: return SDLK_A;
        case BSP_INPUT_SCANCODE_S: return SDLK_S;
        case BSP_INPUT_SCANCODE_D: return SDLK_D;
        case BSP_INPUT_SCANCODE_F: return SDLK_F;
        case BSP_INPUT_SCANCODE_G: return SDLK_G;
        case BSP_INPUT_SCANCODE_H: return SDLK_H;
        case BSP_INPUT_SCANCODE_J: return SDLK_J;
        case BSP_INPUT_SCANCODE_K: return SDLK_K;
        case BSP_INPUT_SCANCODE_L: return SDLK_L;
        case BSP_INPUT_SCANCODE_Z: return SDLK_Z;
        case BSP_INPUT_SCANCODE_X: return SDLK_X;
        case BSP_INPUT_SCANCODE_C: return SDLK_C;
        case BSP_INPUT_SCANCODE_V: return SDLK_V;
        case BSP_INPUT_SCANCODE_B: return SDLK_B;
        case BSP_INPUT_SCANCODE_Y: return SDLK_Y;
        case BSP_INPUT_SCANCODE_N: return SDLK_N;
        case BSP_INPUT_SCANCODE_M: return SDLK_M;
        case BSP_INPUT_SCANCODE_ESCAPED_GREY_UP: return SDLK_UP;
        case BSP_INPUT_SCANCODE_ESCAPED_GREY_DOWN: return SDLK_DOWN;
        case BSP_INPUT_SCANCODE_ESCAPED_GREY_LEFT: return SDLK_LEFT;
        case BSP_INPUT_SCANCODE_ESCAPED_GREY_RIGHT: return SDLK_RIGHT;
        default: return SDLK_UNKNOWN;
    }
}

static void push_key(SDL_Keycode key, bool pressed) {
    if (key == SDLK_UNKNOWN) return;
    static SDL_Keycode last_down_key = SDLK_UNKNOWN;
    static int64_t last_down_us = 0;
    int64_t now_us = esp_timer_get_time();
    if (pressed && key == last_down_key && (now_us - last_down_us) < 100000) return;
    if (pressed) {
        last_down_key = key;
        last_down_us = now_us;
    }
    SDL_Event out = {};
    out.type = pressed ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    out.key.key = key;
    out.key.scancode = SDL_SCANCODE_UNKNOWN;
    SDL_PushEvent(&out);
}

static void push_nav(bsp_input_navigation_key_t key, bool pressed) {
    if (key == BSP_INPUT_NAVIGATION_KEY_VOLUME_UP || key == BSP_INPUT_NAVIGATION_KEY_VOLUME_DOWN) {
        if (!pressed) return;
        hardware_volume += (key == BSP_INPUT_NAVIGATION_KEY_VOLUME_UP) ? 5.0f : -5.0f;
        if (hardware_volume < 0.0f) hardware_volume = 0.0f;
        if (hardware_volume > 100.0f) hardware_volume = 100.0f;
        esp_err_t err = bsp_audio_set_volume(hardware_volume);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "hardware volume %.0f%%", (double)hardware_volume);
        } else {
            ESP_LOGW(TAG, "hardware volume %.0f%% failed: %s", (double)hardware_volume, esp_err_to_name(err));
        }
        return;
    }
    if (key == BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A) {
        push_key(SDLK_SPACE, pressed);
        return;
    }
    if (key == BSP_INPUT_NAVIGATION_KEY_GAMEPAD_B) {
        push_key(SDLK_LALT, pressed);
        return;
    }
    if (key == BSP_INPUT_NAVIGATION_KEY_GAMEPAD_X) {
        push_key(SDLK_RCTRL, pressed);
        return;
    }
    if (key == BSP_INPUT_NAVIGATION_KEY_GAMEPAD_Y) {
        push_key(SDLK_P, pressed);
        return;
    }
    push_key(nav_key(key), pressed);
}

static void *input_task(void *) {
    bsp_input_event_t in;
    while (true) {
        if (xQueueReceive(input_queue, &in, portMAX_DELAY) != pdTRUE) continue;
        if (in.type == INPUT_EVENT_TYPE_KEYBOARD) {
            SDL_Keycode key = (SDL_Keycode)(unsigned char)in.args_keyboard.ascii;
            ESP_LOGI(TAG, "input keyboard ascii=%d key=%d ignored", (int)(unsigned char)in.args_keyboard.ascii, (int)key);
            continue;
        } else if (in.type == INPUT_EVENT_TYPE_NAVIGATION) {
            ESP_LOGI(TAG, "input navigation key=%d state=%d mapped=%d",
                     (int)in.args_navigation.key, (int)in.args_navigation.state,
                     (int)nav_key(in.args_navigation.key));
            if (in.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_F1 && in.args_navigation.state) {
                bsp_device_restart_to_launcher();
                continue;
            }
            push_nav(in.args_navigation.key, in.args_navigation.state);
        } else if (in.type == INPUT_EVENT_TYPE_SCANCODE) {
            SDL_Keycode key = scancode_key(in.args_scancode.scancode);
            bool pressed = (in.args_scancode.scancode & BSP_INPUT_SCANCODE_RELEASE_MODIFIER) == 0;
            ESP_LOGI(TAG, "input scancode raw=0x%x pressed=%d mapped=%d",
                     (unsigned int)in.args_scancode.scancode, (int)pressed, (int)key);
            push_key(key, pressed);
        } else continue;
    }
    return nullptr;
}

extern "C" void app_main(void) {
    gpio_install_isr_service(0);
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    bsp_configuration_t cfg = {};
    cfg.display.requested_color_format = BSP_DISPLAY_COLOR_FORMAT_16_565RGB;
    cfg.display.num_fbs = 1;
    ESP_ERROR_CHECK(bsp_device_initialize(&cfg));
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_queue));

    // bsp_device_initialize() does not turn the backlight on -- that's a
    // separate, opt-in call, and backlight state on the coprocessor can
    // persist unpredictably across app switches. Force it on explicitly so
    // the screen isn't just rendering into darkness.
    bsp_display_set_backlight_brightness(100);

    err = sd_mount();
    if (err != ESP_OK) {
        ESP_LOGE("openjazz", "SD mount failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI("openjazz", "SD mounted");
    }
    esp_pthread_cfg_t input_cfg = esp_pthread_get_default_config();
    input_cfg.stack_size = 4096;
    input_cfg.prio = 8;
    input_cfg.thread_name = "openjazz-input";
    input_cfg.pin_to_core = 1;
    input_cfg.stack_alloc_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    ESP_ERROR_CHECK(esp_pthread_set_cfg(&input_cfg));
    pthread_t input_thread;
    ESP_ERROR_CHECK(pthread_create(&input_thread, nullptr, input_task, nullptr));

    esp_pthread_cfg_t thread_cfg = esp_pthread_get_default_config();
    thread_cfg.stack_size = 32 * 1024;
    thread_cfg.prio = 5;
    thread_cfg.thread_name = "openjazz";
    thread_cfg.pin_to_core = 0;
    thread_cfg.stack_alloc_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
    ESP_ERROR_CHECK(esp_pthread_set_cfg(&thread_cfg));
    pthread_t game_thread;
    ESP_ERROR_CHECK(pthread_create(&game_thread, nullptr, openjazz_thread, nullptr));
    void *thread_result = nullptr;
    ESP_ERROR_CHECK(pthread_join(game_thread, &thread_result));
    int rc = static_cast<int>(reinterpret_cast<intptr_t>(thread_result));
    ESP_LOGI("openjazz", "OpenJazz exited (%d)", rc);
    bsp_device_restart_to_launcher();
}
