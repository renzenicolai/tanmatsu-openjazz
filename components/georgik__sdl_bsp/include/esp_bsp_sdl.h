#pragma once
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { int width, height, pixel_format; size_t max_transfer_sz; bool has_touch; } esp_bsp_sdl_display_config_t;
typedef struct { bool pressed; int x, y; } esp_bsp_sdl_touch_info_t;
esp_err_t esp_bsp_sdl_init(esp_bsp_sdl_display_config_t *, esp_lcd_panel_handle_t *, esp_lcd_panel_io_handle_t *);
esp_err_t esp_bsp_sdl_backlight_on(void);
esp_err_t esp_bsp_sdl_backlight_off(void);
esp_err_t esp_bsp_sdl_display_on_off(bool);
esp_err_t esp_bsp_sdl_touch_init(void);
esp_err_t esp_bsp_sdl_touch_read(esp_bsp_sdl_touch_info_t *);
const char *esp_bsp_sdl_get_board_name(void);
esp_err_t esp_bsp_sdl_deinit(void);
esp_err_t esp_bsp_sdl_blit(int x, int y, int width, int height, const void *pixels);
#ifdef __cplusplus
}
#endif
