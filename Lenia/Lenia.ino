// Lenia continuous cellular automaton //

#include "lcd.h"

#define WIDTH   320
#define HEIGHT  240
#define SCR (WIDTH * HEIGHT)

float mu = 0.15f;
float sigma = 0.015f;
float dt = 0.3f;
int R = 13;

float *field, *nextField, *sat;
uint16_t* frameBuffer;

float randomf(float minf, float maxf) { return minf + (rand() % (1UL << 31)) * (maxf - minf) / (1UL << 31); }
uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }

uint16_t grayLUT[256];

float growth(float x) {

  float diff = (x - mu) / sigma;
  return expf(-0.5f * diff * diff) * 2.0f - 1.0f;

}

void rndseed() {

  mu = randomf(0.13f, 0.20f);
  sigma = randomf(0.012f, 0.022f);

  for (int i = 0; i < 256; i++) grayLUT[i] = color565(i, i, i);
  for(int i = 0; i < SCR; i++) field[i] = (randomf(0.0f, 1.0f) > 0.75f) ? randomf(0.0f, 1.0f) : 0.0f;

}


void setup(){

  srand(read_cycle());

  sysctl_set_spi0_dvp_data(1);
	dmac_init();

  lcd_init(SPI0, 3, 6, 7, 20000000, 37, 38, 3);
	lcd_clear(0xFFFF);


  field = (float*)malloc(SCR * sizeof(float));
  nextField = (float*)malloc(SCR * sizeof(float));
  sat = (float*)malloc(SCR * sizeof(float));
  frameBuffer = (uint16_t*)malloc(SCR * sizeof(uint16_t));

  rndseed();

}

void loop(){

  for (int y = 0; y < HEIGHT; y++) {
    float rowSum = 0;
    for (int x = 0; x < WIDTH; x++) {
      rowSum += field[x + y * WIDTH];
      sat[x + y * WIDTH] = (y > 0 ? sat[x + (y-1) * WIDTH] : 0) + rowSum;
    }
  }

  int r_inner = R;
  int r_side = R * 0.7f; 

  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
            
      int x1a = max(0, x - r_inner), x1b = min(WIDTH - 1, x + r_inner);
      int y1a = max(0, y - r_side),  y1b = min(HEIGHT - 1, y + r_side);
      float s1 = sat[x1b + y1b*WIDTH] - sat[x1a + y1b*WIDTH] - sat[x1b + y1a*WIDTH] + sat[x1a + y1a*WIDTH];
      float a1 = (x1b - x1a) * (y1b - y1a);

      int x2a = max(0, x - r_side),  x2b = min(WIDTH - 1, x + r_side);
      int y2a = max(0, y - r_inner), y2b = min(HEIGHT - 1, y + r_inner);
      float s2 = sat[x2b + y2b*WIDTH] - sat[x2a + y2b*WIDTH] - sat[x2b + y2a*WIDTH] + sat[x2a + y2a*WIDTH];
      float a2 = (x2b - x2a) * (y2b - y2a);

      int x3a = max(0, x - r_side),  x3b = min(WIDTH - 1, x + r_side);
      int y3a = max(0, y - r_side),  y3b = min(HEIGHT - 1, y + r_side);
      float s3 = sat[x3b + y3b*WIDTH] - sat[x3a + y3b*WIDTH] - sat[x3b + y3a*WIDTH] + sat[x3a + y3a*WIDTH];
      float a3 = (x3b - x3a) * (y3b - y3a);

      float area = max(1.0f, a1 + a2 - a3);
      float avg = (s1 + s2 - s3) / area;
      float val = field[x + y * WIDTH] + dt * growth(avg);
      nextField[x + y * WIDTH] = constrain(val, 0.0f, 1.0f);
      frameBuffer[x + y * WIDTH] = grayLUT[(uint8_t)(nextField[x + y * WIDTH] * 255.0f)];

    }
  }

  float* temp = field; 
  field = nextField; 
  nextField = temp;
  
  lcd_draw_picture(0, 0, WIDTH, HEIGHT, (uint16_t*)frameBuffer);                                                  
  
}