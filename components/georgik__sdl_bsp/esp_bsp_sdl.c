#include "esp_bsp_sdl.h"
#include "bsp/display.h"
#define SDL_PIXELFORMAT_RGB565 0x15151002u
static size_t w, h;
esp_err_t esp_bsp_sdl_init(esp_bsp_sdl_display_config_t *c, esp_lcd_panel_handle_t *p, esp_lcd_panel_io_handle_t *io) {
    bsp_display_color_format_t f; bsp_display_endianness_t e;
    esp_err_t r = bsp_display_get_parameters(&w, &h, &f, &e); if (r != ESP_OK) return r;
    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_io_handle_t panel_io = NULL;
    r = bsp_display_get_panel(&panel); if (r != ESP_OK) return r;
    r = bsp_display_get_panel_io(&panel_io); if (r != ESP_OK) return r;
    /* The panel is portrait in memory; Tanmatsu is used landscape. */
    c->width=(int)h; c->height=(int)w; c->pixel_format=SDL_PIXELFORMAT_RGB565;
    c->max_transfer_sz=w*h*2; c->has_touch=false; *p=panel; *io=panel_io; return ESP_OK;
}
esp_err_t esp_bsp_sdl_blit(int x,int y,int width,int height,const void *pixels){return bsp_display_blit(x,y,x+width,y+height,pixels);}
esp_err_t esp_bsp_sdl_backlight_on(void){return bsp_display_set_backlight_brightness(100);}
esp_err_t esp_bsp_sdl_backlight_off(void){return bsp_display_set_backlight_brightness(0);}
esp_err_t esp_bsp_sdl_display_on_off(bool on){return on?esp_bsp_sdl_backlight_on():esp_bsp_sdl_backlight_off();}
esp_err_t esp_bsp_sdl_touch_init(void){return ESP_ERR_NOT_SUPPORTED;}
esp_err_t esp_bsp_sdl_touch_read(esp_bsp_sdl_touch_info_t *i){(void)i;return ESP_ERR_NOT_SUPPORTED;}
const char *esp_bsp_sdl_get_board_name(void){return "Tanmatsu";}
esp_err_t esp_bsp_sdl_deinit(void){return ESP_OK;}
