// Lenia continuous cellular automaton - dual core //

#include "bsp.h"
#include "lcd.h"

#define WIDTH   320
#define HEIGHT  240
#define SCR (WIDTH * HEIGHT)

float mu = 0.15f;
float sigma = 0.015f;
float dt = 0.3f;
int R = 13;

float *field, *nextField;
uint16_t* frameBuffer;
uint16_t grayLUT[256];

volatile int core1_ready = 1;

float randomf(float minf, float maxf) { return minf + (rand() % (1UL << 31)) * (maxf - minf) / (1UL << 31); }
uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }

#define LUT_SIZE 1024
float growthLUT[LUT_SIZE];

void precache_growth() {

  for (int i = 0; i < LUT_SIZE; i++) {
    float x = (float)i / (LUT_SIZE - 1);
    float diff = (x - mu) / sigma;
    growthLUT[i] = expf(-0.5f * diff * diff) * 2.0f - 1.0f;
  }

}

void rndseed() {

    precache_growth();

    mu = randomf(0.13f, 0.20f);
    sigma = randomf(0.012f, 0.022f);

    for (int i = 0; i < 256; i++) grayLUT[i] = color565(i, i, i);
    for(int i = 0; i < SCR; i++) field[i] = (randomf(0.0f, 1.0f) > 0.75f) ? randomf(0.0f, 1.0f) : 0.0f;

}

void compute_lenia_section(int startY, int endY) {

  float r_sq = (float)R * R;
  for (int y = startY; y < endY; y++) {

    int rowOff_y = y * WIDTH;
    for (int x = 0; x < WIDTH; x++) {

      float total_sum = 0.0f;
      int count = 0;

      int y_min = max(0, y - R);
      int y_max = min(HEIGHT - 1, y + R);
      int x_min = max(0, x - R);
      int x_max = min(WIDTH - 1, x + R);

      for (int ky = y_min; ky <= y_max; ky++) {
        float dy_sq = (float)(ky - y) * (float)(ky - y);
        int rowOff_ky = ky * WIDTH;
        for (int kx = x_min; kx <= x_max; kx++) {
          float dx = (float)(kx - x);
          if (dx * dx + dy_sq <= r_sq) {
            total_sum += field[kx + rowOff_ky];
            count++;
          }
        }
      }

      float avg = (count > 0) ? (total_sum / count) : 0;
      int lut_idx = (int)(avg * (LUT_SIZE - 1));
      if (lut_idx < 0) lut_idx = 0;
      if (lut_idx >= LUT_SIZE) lut_idx = LUT_SIZE - 1;

      float g_val = growthLUT[lut_idx];
      float val = field[x + rowOff_y] + dt * g_val;
      float res = constrain(val, 0.0f, 1.0f);
            
      nextField[x + rowOff_y] = res;
      frameBuffer[x + rowOff_y] = grayLUT[(uint8_t)(res * 255.0f)];

    }
  }

}

void setup() {

    srand(read_cycle());
    
    sysctl_set_spi0_dvp_data(1);
    dmac_init();
    lcd_init(SPI0, 3, 6, 7, 20000000, 37, 38, 3);
    lcd_clear(0xFFFF);

    field = (float*)malloc(SCR * sizeof(float));
    nextField = (float*)malloc(SCR * sizeof(float));
    frameBuffer = (uint16_t*)malloc(SCR * sizeof(uint16_t));

    rndseed();
    register_core1(loop1, NULL);

}

void loop() {

    core1_ready = 0;
    compute_lenia_section(0, HEIGHT / 2);
    while(core1_ready == 0);

    lcd_draw_picture(0, 0, WIDTH, HEIGHT, frameBuffer);
    
    float* temp = field;
    field = nextField;
    nextField = temp;

}

int loop1(void *parameter) {

    while(1) {

        while(core1_ready == 1);
        compute_lenia_section(HEIGHT / 2, HEIGHT);
        core1_ready = 1;

    }

    return 0;

}