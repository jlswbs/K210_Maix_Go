// Multi view variable Complex Ginzburg-Landau reaction diffusion //

#include "lcd.h"

#define WIDTH 320
#define HEIGHT 240
#define SCR (WIDTH * HEIGHT)

float dt = 0.1f;

float *re, *im;
int offset_p = 0;
int offset_q = SCR;
uint16_t* frameBuffer;

float randomf(float minf, float maxf) { return minf + (rand() % (1UL << 31)) * (maxf - minf) / (1UL << 31); }
uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }

#define GRID_W 4
#define GRID_H 3

#define CELL_W (WIDTH  / GRID_W)
#define CELL_H (HEIGHT / GRID_H)

float grid_b[GRID_W][GRID_H];
float grid_c[GRID_W][GRID_H];

float grid_hue[GRID_W][GRID_H];
float grid_hue_range[GRID_W][GRID_H];
float grid_sat[GRID_W][GRID_H];
int   grid_cmode[GRID_W][GRID_H];

void hsv_to_rgb(float h, float s, float v, uint8_t *r, uint8_t *g, uint8_t *b) {

    h = fmodf(h, 360.0f);
    if (h < 0.0f) h += 360.0f;

    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;

    float rr, gg, bb;
    if      (h < 60.0f)  { rr = c; gg = x; bb = 0; }
    else if (h < 120.0f) { rr = x; gg = c; bb = 0; }
    else if (h < 180.0f) { rr = 0; gg = c; bb = x; }
    else if (h < 240.0f) { rr = 0; gg = x; bb = c; }
    else if (h < 300.0f) { rr = x; gg = 0; bb = c; }
    else                 { rr = c; gg = 0; bb = x; }

    *r = (uint8_t)((rr + m) * 255.0f);
    *g = (uint8_t)((gg + m) * 255.0f);
    *b = (uint8_t)((bb + m) * 255.0f);

}

void rndseed() {

    offset_p = 0;
    offset_q = SCR;

    float base_hue = randomf(0.0f, 360.0f);
    
    for (int gy = 0; gy < GRID_H; gy++) {
        for (int gx = 0; gx < GRID_W; gx++) {
            if ((gx + gy) % 3 == 0) {
                grid_b[gx][gy] = randomf(0.6f, 1.4f);
                grid_c[gx][gy] = randomf(-1.8f, -0.8f);
            } else if ((gx + gy) % 3 == 1) {
                grid_b[gx][gy] = randomf(-1.2f, -0.4f);
                grid_c[gx][gy] = randomf(0.3f, 1.2f);
            } else {
                grid_b[gx][gy] = randomf(-0.5f, 0.8f);
                grid_c[gx][gy] = randomf(-1.0f, 1.5f);
            }
            
            grid_hue[gx][gy] = fmodf(base_hue + gx * 40.0f + gy * 70.0f, 360.0f);
            grid_hue_range[gx][gy] = randomf(25.0f, 100.0f);
            grid_sat[gx][gy] = randomf(0.7f, 1.0f);
            grid_cmode[gx][gy] = (gx * 2 + gy) % 4;
        }
    }

    for (int i = 0; i < SCR * 2; i++) {
        re[i] = randomf(-1.0f, 1.0f);
        im[i] = randomf(-1.0f, 1.0f);
    }

}

void setup() {

    srand(read_cycle());

    sysctl_set_spi0_dvp_data(1);
    dmac_init();

    lcd_init(SPI0, 3, 6, 7, 20000000, 37, 38, 3);
    lcd_clear(0xFFFF);

    re          = (float*)malloc(SCR * 2 * sizeof(float));
    im          = (float*)malloc(SCR * 2 * sizeof(float));
    frameBuffer = (uint16_t*)malloc(SCR * sizeof(uint16_t));

    rndseed();

}

void loop() {

    float   *rep    = re + offset_p;
    float   *imp    = im + offset_p;
    float   *req_ptr = re + offset_q;
    float   *imq_ptr = im + offset_q;
    uint16_t *fb_ptr = frameBuffer;

    for (int y = 0; y < HEIGHT; y++) {

        int y_up  = ((y - 1 + HEIGHT) % HEIGHT) * WIDTH;
        int y_mid = y * WIDTH;
        int y_dn  = ((y + 1) % HEIGHT) * WIDTH;

        for (int x = 0; x < WIDTH; x++) {

            float best_w     = 0.0f;
            float best_b     = grid_b[0][0];
            float best_c_val = grid_c[0][0];
            float best_hue   = grid_hue[0][0];
            float best_hrange= grid_hue_range[0][0];
            float best_sat   = grid_sat[0][0];
            int   best_cmode = grid_cmode[0][0];

            for (int gy = 0; gy < GRID_H; gy++) {
                for (int gx = 0; gx < GRID_W; gx++) {
                    float cx = (gx + 0.5f) * CELL_W;
                    float cy = (gy + 0.5f) * CELL_H;
                    float dx = x - cx;
                    float dy = y - cy;
                    float w  = 1.0f / (dx*dx + dy*dy + 1.0f);
                    if (w > best_w) {
                        best_w      = w;
                        best_b      = grid_b[gx][gy];
                        best_c_val  = grid_c[gx][gy];
                        best_hue    = grid_hue[gx][gy];
                        best_hrange = grid_hue_range[gx][gy];
                        best_sat    = grid_sat[gx][gy];
                        best_cmode  = grid_cmode[gx][gy];
                    }
                }
            }

            int x_l = (x - 1 + WIDTH) % WIDTH;
            int x_r = (x + 1) % WIDTH;

            float lapRe = (rep[x_l + y_mid] + rep[x_r + y_mid] +
                           rep[x + y_up]    + rep[x + y_dn]    -
                           4.0f * rep[x + y_mid]);
            float lapIm = (imp[x_l + y_mid] + imp[x_r + y_mid] +
                           imp[x + y_up]    + imp[x + y_dn]    -
                           4.0f * imp[x + y_mid]);

            float R     = rep[x + y_mid];
            float I     = imp[x + y_mid];
            float sqMag = R*R + I*I;

            float dR = R - (R - best_c_val * I) * sqMag + (lapRe - best_b * lapIm);
            float dI = I - (I + best_c_val * R) * sqMag + (lapIm + best_b * lapRe);

            float nRe = R + dR * dt;
            float nIm = I + dI * dt;

            nRe = (nRe < -1.5f) ? -1.5f : (nRe > 1.5f ? 1.5f : nRe);
            nIm = (nIm < -1.5f) ? -1.5f : (nIm > 1.5f ? 1.5f : nIm);

            *req_ptr++ = nRe;
            *imq_ptr++ = nIm;

            float amp   = sqrtf(sqMag);
            float phase = atan2f(nIm, nRe);
            float h_shift;

            switch (best_cmode) {
                case 0: h_shift = (phase / 3.14159f) * best_hrange; break;
                case 1: h_shift = amp * best_hrange * 1.5f; break;
                case 2: h_shift = (nRe - nIm) * best_hrange * 0.8f; break;
                case 3: h_shift = (nRe * nIm) * best_hrange * 3.0f; break;
                default: h_shift = 0.0f; break;
            }

            float final_hue = best_hue + h_shift;
            float final_sat = best_sat;
            float final_val = 0.25f + amp * 0.6f;
            if (final_val > 1.0f) final_val = 1.0f;

            uint8_t r_col, g_col, b_col_out;
            hsv_to_rgb(final_hue, final_sat, final_val, &r_col, &g_col, &b_col_out);

            *fb_ptr++ = color565(r_col, g_col, b_col_out);

        }
    }

    offset_p = SCR - offset_p;
    offset_q = SCR - offset_q;

    lcd_draw_picture(0, 0, WIDTH, HEIGHT, (uint16_t*)frameBuffer);

}