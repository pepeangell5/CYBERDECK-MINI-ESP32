#ifndef DISPLAY_CONFIG_H
#define DISPLAY_CONFIG_H
#include <LovyanGFX.hpp>
extern LGFX_Sprite* globalCanvas;
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789  _panel_instance; 
  lgfx::Bus_SPI       _bus_instance;
public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 20000000;
      cfg.pin_sclk = 12; cfg.pin_mosi = 11; cfg.pin_miso = 13; cfg.pin_dc = 21;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs = 10; cfg.pin_rst = 47;
      cfg.panel_width = 240; cfg.panel_height = 320;
      cfg.offset_x = 0; cfg.offset_y = 0;
      _panel_instance.config(cfg);
    }
    setPanel(&_panel_instance);
  }
};

#endif