// Variable Complex Ginzburg-Landau reaction diffusion //

#include "lcd.h"

#define WIDTH 320
#define HEIGHT 240
#define SCR (WIDTH * HEIGHT)

float param_b = 1.0f;
float param_c = -1.5f;
float dt = 0.1f;

float *re, *im;
int offset_p = 0;
int offset_q = SCR;
uint16_t* frameBuffer;

float randomf(float minf, float maxf) { return minf + (rand() % (1UL << 31)) * (maxf - minf) / (1UL << 31); }
uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }

float b_start, b_end;
float c_start, c_end;

#define GRID_W 8
#define GRID_H 6
float grid_b[GRID_W][GRID_H];
float grid_c[GRID_W][GRID_H];

float grid_r_col[GRID_W][GRID_H];
float grid_g_col[GRID_W][GRID_H];
float grid_b_col[GRID_W][GRID_H];

void rndseed() {

  offset_p = 0;
  offset_q = SCR;

  for(int gy = 0; gy < GRID_H; gy++) {
    for(int gx = 0; gx < GRID_W; gx++) {
      grid_b[gx][gy] = randomf(0.01f, 1.35f);
      grid_c[gx][gy] = randomf(-1.35f, -0.01f);
      grid_r_col[gx][gy] = randomf(50.0f, 120.0f);
      grid_g_col[gx][gy] = randomf(80.0f, 150.0f);
      grid_b_col[gx][gy] = randomf(50.0f, 120.0f);
    }
  }

  for(int i = 0; i < SCR * 2; i++){
    re[i] = randomf(-0.5f, 0.5f);
    im[i] = randomf(-0.5f, 0.5f);
  }

}

void setup(){

  srand(read_cycle());

  sysctl_set_spi0_dvp_data(1);
  dmac_init();

  lcd_init(SPI0, 3, 6, 7, 20000000, 37, 38, 3);
  lcd_clear(0xFFFF);

  re = (float*)malloc(SCR * 2 * sizeof(float));
  im = (float*)malloc(SCR * 2 * sizeof(float));
  frameBuffer = (uint16_t*)malloc(SCR * sizeof(uint16_t));

  rndseed();

}

void loop(){

  float *rep = re + offset_p;
  float *imp = im + offset_p;
  float *req_ptr = re + offset_q;
  float *imq_ptr = im + offset_q;
  uint16_t *fb_ptr = frameBuffer;

  for (int y = 0; y < HEIGHT; y++) {

    float gy_map = (float)y / (HEIGHT - 1) * (GRID_H - 1);
    int y0 = (int)gy_map;
    int y1 = (y0 < GRID_H - 1) ? y0 + 1 : y0;
    float fy = gy_map - y0;

    int y_up  = ((y - 1 + HEIGHT) % HEIGHT) * WIDTH;
    int y_mid = y * WIDTH;
    int y_dn  = ((y + 1) % HEIGHT) * WIDTH;

    for (int x = 0; x < WIDTH; x++) {

      float gx_map = (float)x / (WIDTH - 1) * (GRID_W - 1);
      int x0 = (int)gx_map;
      int x1 = (x0 < GRID_W - 1) ? x0 + 1 : x0;
      float fx = gx_map - x0;

      float b00 = grid_b[x0][y0];
      float b10 = grid_b[x1][y0];
      float b01 = grid_b[x0][y1];
      float b11 = grid_b[x1][y1];
      float local_b = (1-fx)*(1-fy)*b00 + fx*(1-fy)*b10 + (1-fx)*fy*b01 + fx*fy*b11;

      float c00 = grid_c[x0][y0];
      float c10 = grid_c[x1][y0];
      float c01 = grid_c[x0][y1];
      float c11 = grid_c[x1][y1];
      float local_c = (1-fx)*(1-fy)*c00 + fx*(1-fy)*c10 + (1-fx)*fy*c01 + fx*fy*c11;

      int x_l = (x - 1 + WIDTH) % WIDTH;
      int x_r = (x + 1) % WIDTH;

      float lapRe = (rep[x_l + y_mid] + rep[x_r + y_mid] + rep[x + y_up] + rep[x + y_dn] - 4.0f * rep[x + y_mid]);
      float lapIm = (imp[x_l + y_mid] + imp[x_r + y_mid] + imp[x + y_up] + imp[x + y_dn] - 4.0f * imp[x + y_mid]);

      float R = rep[x + y_mid];
      float I = imp[x + y_mid];
      float sqMag = R*R + I*I;

      float dR = R - (R - local_c * I) * sqMag + (lapRe - local_b * lapIm);
      float dI = I - (I + local_c * R) * sqMag + (lapIm + local_b * lapRe);

      float nRe = R + dR * dt;
      float nIm = I + dI * dt;

      nRe = (nRe < -1.5f) ? -1.5f : (nRe > 1.5f ? 1.5f : nRe);
      nIm = (nIm < -1.5f) ? -1.5f : (nIm > 1.5f ? 1.5f : nIm);

      *req_ptr++ = nRe;
      *imq_ptr++ = nIm;

      float local_r = (1-fx)*(1-fy)*grid_r_col[x0][y0] + fx*(1-fy)*grid_r_col[x1][y0] + (1-fx)*fy*grid_r_col[x0][y1] + fx*fy*grid_r_col[x1][y1];
      float local_g = (1-fx)*(1-fy)*grid_g_col[x0][y0] + fx*(1-fy)*grid_g_col[x1][y0] + (1-fx)*fy*grid_g_col[x0][y1] + fx*fy*grid_g_col[x1][y1];
      float local_bl = (1-fx)*(1-fy)*grid_b_col[x0][y0] + fx*(1-fy)*grid_b_col[x1][y0] + (1-fx)*fy*grid_b_col[x0][y1] + fx*fy*grid_b_col[x1][y1];

      uint8_t r_col = (uint8_t)((nRe + 1.2f) * local_r);
      uint8_t g_col = (uint8_t)(sqMag * local_g);
      uint8_t b_col = (uint8_t)((nIm + 1.2f) * local_bl);

      *fb_ptr++ = color565(r_col, g_col, b_col);

    }

  }

  offset_p = SCR - offset_p;
  offset_q = SCR - offset_q;
    
  lcd_draw_picture(0, 0, WIDTH, HEIGHT, (uint16_t*)frameBuffer);
                                                  
}