// Variable Lenia continuous cellular automaton //

#include "lcd.h"

#define WIDTH   320
#define HEIGHT  240
#define SCR (WIDTH * HEIGHT)

#define FOCUS_COLS 3
#define FOCUS_ROWS 4
#define NUM_FOCUSES (FOCUS_COLS * FOCUS_ROWS)

struct Focus {
    float x, y;
    float mu, sigma;
    float r, g, b;
    float R;
};

Focus focuses[NUM_FOCUSES];
float *pixelMu, *pixelSigma, *pixelRad; 

float base_mu = 0.15f;
float base_sigma = 0.015f;
float dt = 0.3f;

float *field, *nextField, *sat;
uint16_t* frameBuffer;

float randomf(float minf, float maxf) { return minf + (rand() % (1UL << 31)) * (maxf - minf) / (1UL << 31); }
uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }

uint16_t grayLUT[256];

inline float growth(float x, float m, float s) {

  float diff = (x - m) / s;
  return expf(-0.5f * diff * diff) * 2.0f - 1.0f;

}

void rndseed() {

  for (int i = 0; i < 256; i++) grayLUT[i] = color565(i, i, i);

  float cellW = (float)WIDTH / FOCUS_COLS;
  float cellH = (float)HEIGHT / FOCUS_ROWS;

  for (int r = 0; r < FOCUS_ROWS; r++) {
    for (int c = 0; c < FOCUS_COLS; c++) {
      int i = r * FOCUS_COLS + c;
      focuses[i].x = (c * cellW) + (cellW / 2.0f);
      focuses[i].y = (r * cellH) + (cellH / 2.0f);
            
      focuses[i].mu = base_mu + randomf(-0.1f, 0.1f);
      focuses[i].sigma = base_sigma + randomf(-0.01f, 0.01f);
      focuses[i].R = randomf(6.0f, 16.0f);
            
    }
  }

  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      int idx = x + y * WIDTH;
      float totalW = 0, smu = 0, ssigma = 0, sr = 0, sg = 0, sb = 0, sRad = 0;

      for (int f = 0; f < NUM_FOCUSES; f++) {
        float d2 = powf(x - focuses[f].x, 2) + powf(y - focuses[f].y, 2);
        float w = 1.0f / (d2 + 40.0f);
        smu += focuses[f].mu * w;
        ssigma += focuses[f].sigma * w;
        sr += focuses[f].r * w;
        sg += focuses[f].g * w;
        sb += focuses[f].b * w;
        sRad += focuses[f].R * w;
        totalW += w;
      }
      
      pixelMu[idx] = smu / totalW;
      pixelSigma[idx] = ssigma / totalW;
      pixelRad[idx] = sRad / totalW;
            
      field[idx] = (randomf(0.0f, 1.0f) > 0.75f) ? randomf(0.0f, 1.0f) : 0.0f;

    }
  }

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
  pixelMu = (float*)malloc(SCR * sizeof(float));
  pixelSigma = (float*)malloc(SCR * sizeof(float));
  pixelRad = (float*)malloc(SCR * sizeof(float));
  frameBuffer = (uint16_t*)malloc(SCR * sizeof(uint16_t));

  rndseed();

}

void loop(){

  for (int y = 0; y < HEIGHT; y++) {
    float rowSum = 0;
    for (int x = 0; x < WIDTH; x++) {
      int idx = x + y * WIDTH;
      rowSum += field[idx];
      sat[idx] = (y > 0 ? sat[x + (y-1) * WIDTH] : 0) + rowSum;
    }
  }

  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      int idx = x + y * WIDTH;
            
      float curR = pixelRad[idx];
      int r_in = (int)curR;
      int r_s = (int)(curR * 0.7f); 

      auto getSum = [&](int rx, int ry) {
        int x1 = max(0, x - rx), x2 = min(WIDTH - 1, x + rx);
        int y1 = max(0, y - ry), y2 = min(HEIGHT - 1, y + ry);
        return (sat[x2 + y2*WIDTH] - sat[x1 + y2*WIDTH] - sat[x2 + y1*WIDTH] + sat[x1 + y1*WIDTH]);
      };

      float s1 = getSum(r_in, r_s); float a1 = (2*r_in) * (2*r_s);
      float s2 = getSum(r_s, r_in); float a2 = (2*r_s) * (2*r_in);
      float s3 = getSum(r_s, r_s);  float a3 = (2*r_s) * (2*r_s);

      float area = max(1.0f, a1 + a2 - a3);
      float avg = (s1 + s2 - s3) / area;
            
      float val = field[idx] + dt * growth(avg, pixelMu[idx], pixelSigma[idx]);
      nextField[idx] = constrain(val, 0.0f, 1.0f);
            
      frameBuffer[idx] = grayLUT[(uint8_t)(255 * field[idx])];
        
    }
  }

  float* t = field; field = nextField; nextField = t;
  
  lcd_draw_picture(0, 0, WIDTH, HEIGHT, (uint16_t*)frameBuffer);                                                  
  
}