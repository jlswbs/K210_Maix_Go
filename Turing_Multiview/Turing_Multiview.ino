// Multi view variable Multi-scale Turing patterns // 

#include "lcd.h"

#define WIDTH 320
#define HEIGHT 240
#define SCR (WIDTH * HEIGHT)

#define GRID_COLS 4
#define GRID_ROWS 3
#define NUM_REGIONS (GRID_COLS * GRID_ROWS)

#define REGION_WIDTH (WIDTH / GRID_COLS)
#define REGION_HEIGHT (HEIGHT / GRID_ROWS)
#define REGION_SCR (REGION_WIDTH * REGION_HEIGHT)

typedef struct {
    float *gridA;
    float *gridB;
    float *currentGrid;
    float *nextGrid;
    float *blurBuffer;
    float *bestVariation;
    float *inhibitor;
    int *bestLevel;
    bool *direction;

    float base;
    float stepScale;
    float stepOffset;
    float blurFactor;
    int levels;
    int blurlevels;
    int radii[20];
    float stepSizes[20];

    int offsetX;
    int offsetY;
} Region;

Region regions[NUM_REGIONS];
uint16_t* frameBuffer;

float randomf(float minf, float maxf) { return minf + ((float)rand() / (float)RAND_MAX) * (maxf - minf); }
uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }

uint16_t grayLUT[256];

void initRegion(Region *r, int regionIdx) {

    r->offsetX = (regionIdx % GRID_COLS) * REGION_WIDTH;
    r->offsetY = (regionIdx / GRID_COLS) * REGION_HEIGHT;

    r->gridA         = (float*)malloc(REGION_SCR * sizeof(float));
    r->gridB         = (float*)malloc(REGION_SCR * sizeof(float));
    r->blurBuffer    = (float*)malloc(REGION_SCR * sizeof(float));
    r->bestVariation = (float*)malloc(REGION_SCR * sizeof(float));
    r->inhibitor     = (float*)malloc(REGION_SCR * sizeof(float));
    r->bestLevel     = (int*)malloc(REGION_SCR * sizeof(int));
    r->direction     = (bool*)malloc(REGION_SCR * sizeof(bool));

    r->currentGrid = r->gridA;
    r->nextGrid = r->gridB;

    r->base = randomf(1.4f, 2.5f);
    r->stepScale = randomf(0.01f, 0.06f);
    r->stepOffset = randomf(0.01f, 0.03f);
    r->blurFactor = randomf(0.5f, 0.8f);
    r->levels = 7;

    r->blurlevels = (int)((r->levels + 1) * r->blurFactor);

    int maxRadius = fminf(REGION_WIDTH, REGION_HEIGHT) / 2;

    for (int i = 0; i < r->levels; i++) {
        r->radii[i] = fminf((int)powf(r->base, i), maxRadius);
        r->stepSizes[i] = logf(r->radii[i] + 1) * r->stepScale + r->stepOffset;
    }

    for (int i = 0; i < REGION_SCR; i++) {
        r->currentGrid[i] = randomf(-1.0f, 1.0f);
        r->bestVariation[i] = 1e38f;
    }

}

void setup() {

    srand(read_cycle());

    sysctl_set_spi0_dvp_data(1);
    dmac_init();

    lcd_init(SPI0, 3, 6, 7, 20000000, 37, 38, 3);
    lcd_clear(0xFFFF);

    for (int i = 0; i < 256; i++) grayLUT[i] = color565(i, i, i);

    frameBuffer = (uint16_t*)malloc(SCR * sizeof(uint16_t));

    for (int i = 0; i < NUM_REGIONS; i++) {
        initRegion(&regions[i], i);
    }

}

void updateRegion(Region *r) {

    int w = REGION_WIDTH;
    int h = REGION_HEIGHT;
    int scr = REGION_SCR;

    float *currentActivator = r->currentGrid;
    float *currentInhibitor = r->inhibitor;

    for (int lvl = 0; lvl < r->levels - 1; lvl++) {
        int radius = r->radii[lvl];

        if (lvl <= r->blurlevels) {
            float *p_blur = r->blurBuffer;
            float *p_act = currentActivator;
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    int idx = y * w + x;
                    if (y == 0 && x == 0) p_blur[idx] = p_act[idx];
                    else if (y == 0) p_blur[idx] = p_blur[idx - 1] + p_act[idx];
                    else if (x == 0) p_blur[idx] = p_blur[idx - w] + p_act[idx];
                    else p_blur[idx] = p_blur[idx - 1] + p_blur[idx - w] - p_blur[idx - w - 1] + p_act[idx];
                }
            }
        }

        for (int y = 0; y < h; y++) {
            int miny = (y - radius < 0) ? 0 : y - radius;
            int maxy = (y + radius >= h) ? h - 1 : y + radius;
            for (int x = 0; x < w; x++) {
                int minx = (x - radius < 0) ? 0 : x - radius;
                int maxx = (x + radius >= w) ? w - 1 : x + radius;
                int area = (maxx - minx + 1) * (maxy - miny + 1);
                int idx = y * w + x;

                float inh = (r->blurBuffer[maxy * w + maxx] - 
                           (minx > 0 ? r->blurBuffer[maxy * w + (minx-1)] : 0) -
                           (miny > 0 ? r->blurBuffer[(miny-1) * w + maxx] : 0) +
                           ((minx > 0 && miny > 0) ? r->blurBuffer[(miny-1) * w + (minx-1)] : 0)) / area;

                float v = fabsf(currentActivator[idx] - inh);
                if (lvl == 0 || v < r->bestVariation[idx]) {
                    r->bestVariation[idx] = v;
                    r->bestLevel[idx] = lvl;
                    r->direction[idx] = currentActivator[idx] > inh;
                }
                currentInhibitor[idx] = inh;
            }
        }

        currentActivator = currentInhibitor;
    }

    float smallest = 1e38f;
    float largest = -1e38f;

    for (int i = 0; i < scr; i++) {
        float move = r->stepSizes[r->bestLevel[i]];
        if (r->direction[i]) r->nextGrid[i] = r->currentGrid[i] + move;
        else r->nextGrid[i] = r->currentGrid[i] - move;

        if (r->nextGrid[i] < smallest) smallest = r->nextGrid[i];
        if (r->nextGrid[i] > largest) largest = r->nextGrid[i];
    }

    float range = largest - smallest;
    if (range < 0.0001f) range = 0.0001f;

    for (int i = 0; i < scr; i++) {
        r->nextGrid[i] = (r->nextGrid[i] - smallest) / range * 2.0f - 1.0f;
    }

    float *tmp = r->currentGrid;
    r->currentGrid = r->nextGrid;
    r->nextGrid = tmp;

}

void renderRegionsToFramebuffer() {

    memset(frameBuffer, 0xFF, SCR * sizeof(uint16_t));

    for (int rIdx = 0; rIdx < NUM_REGIONS; rIdx++) {
        Region *r = &regions[rIdx];
        int w = REGION_WIDTH;
        int h = REGION_HEIGHT;
        int offsetX = r->offsetX;
        int offsetY = r->offsetY;

        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                int regionIdx = y * w + x;
                float val = r->currentGrid[regionIdx];

                uint8_t gray = (uint8_t)((val + 1.0f) * 127.0f);

                int fbX = offsetX + x;
                int fbY = offsetY + y;
                int fbIdx = fbY * WIDTH + fbX;

                frameBuffer[fbIdx] = grayLUT[gray];
            }
        }

        for (int x = 0; x < w; x++) {
            int fbIdxTop = offsetY * WIDTH + (offsetX + x);
            int fbIdxBottom = (offsetY + h - 1) * WIDTH + (offsetX + x);
            frameBuffer[fbIdxTop] = 0x0000;
            frameBuffer[fbIdxBottom] = 0x0000;
        }
        for (int y = 0; y < h; y++) {
            int fbIdxLeft = (offsetY + y) * WIDTH + offsetX;
            int fbIdxRight = (offsetY + y) * WIDTH + (offsetX + w - 1);
            frameBuffer[fbIdxLeft] = 0x0000;
            frameBuffer[fbIdxRight] = 0x0000;
        }
    }

}

void loop() {

    for (int i = 0; i < NUM_REGIONS; i++) updateRegion(&regions[i]);

    renderRegionsToFramebuffer();

    lcd_draw_picture(0, 0, WIDTH, HEIGHT, (uint16_t*)frameBuffer);

}