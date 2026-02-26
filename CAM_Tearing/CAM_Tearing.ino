// CAM to LCD - Byte Tearing retro video effect //

#include "lcd.h"
#include "ov2640.h"

#define WIDTH 320
#define HEIGHT 240
#define SCR (WIDTH * HEIGHT)

uint16_t* framebuffer;

void setup() {

  srand(read_cycle());

  sysctl_set_spi0_dvp_data(1);
  dmac_init();
  dvpInit(24000000);

  lcd_init(SPI0, 3, 6, 7, 20000000, 37, 38, 3);
  lcd_clear(0xFFFF);

  int a = cambus_scan();
  Sipeed_OV2640_begin(a, 24000000);

  OV2640_write_reg(a, 0xFF, 0x01); // Bank 1
  OV2640_write_reg(a, 0x13, 0x00); // AGC-AEC auto off
  OV2640_write_reg(a, 0x00, 0xFF); // Gain 0-255
  OV2640_write_reg(a, 0x10, 0xFF); // AEC 0-255

  Sipeed_OV2640_run(1);
  framebuffer = get_k210_dataBuffer();

}

void loop() {

  Sipeed_OV2640_sensor_snapshot();

  uint8_t* raw_data = (uint8_t*)framebuffer;
for (int i = 0; i < WIDTH * HEIGHT * 2 - 1; i++) {
    // Čteme o 1 bajt vedle – to rozhodí složky RGB
    raw_data[i] = raw_data[i + 1]; 
}

    
  lcd_draw_picture(0, 0, WIDTH, HEIGHT, framebuffer);

}