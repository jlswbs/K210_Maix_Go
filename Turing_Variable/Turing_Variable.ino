// Variable Multi-scale Turing patterns // 

#include "lcd.h"

#define WIDTH 320
#define HEIGHT 240
#define SCR (WIDTH * HEIGHT)

float *gridA, *gridB;
float *currentGrid, *nextGrid;
float *blurBuffer, *bestVariation, *inhibitor;
int *bestLevel;
bool *direction;

int radii[WIDTH];
float stepSizes[WIDTH];
int levels, blurlevels;
float base, stepScale, stepOffset, blurFactor;

uint16_t* frameBuffer;

float randomf(float minf, float maxf) { return minf + ((float)rand() / (float)RAND_MAX) * (maxf - minf); }
uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }

uint16_t grayLUT[256];

#define FOCUS_COLS 4
#define FOCUS_ROWS 3
#define NUM_FOCUSES (FOCUS_COLS * FOCUS_ROWS)

struct Focus {
    float x, y;
    float base, stepScale, stepOffset;
};

Focus focuses[NUM_FOCUSES];

float *pixelBase, *pixelStepScale, *pixelStepOffset;

void rndseed() {

    for (int i = 0; i < 256; i++) grayLUT[i] = color565(i, i, i);

    float cellW = (float)WIDTH / FOCUS_COLS;
    float cellH = (float)HEIGHT / FOCUS_ROWS;

    for (int r = 0; r < FOCUS_ROWS; r++) {
        for (int c = 0; c < FOCUS_COLS; c++) {
            int i = r * FOCUS_COLS + c;
            focuses[i].x = (c * cellW) + (cellW / 2.0f);
            focuses[i].y = (r * cellH) + (cellH / 2.0f);
            focuses[i].base = randomf(1.4f, 2.6f);
            focuses[i].stepScale = randomf(0.01f, 0.08f);
            focuses[i].stepOffset = randomf(0.01f, 0.04f);
        }
    }

    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int idx = y * WIDTH + x;

            float totalWeight = 0;
            float sumBase = 0;
            float sumScale = 0;
            float sumOffset = 0;

            for (int f = 0; f < NUM_FOCUSES; f++) {
                float dx = x - focuses[f].x;
                float dy = y - focuses[f].y;
                float d2 = dx*dx + dy*dy;

                float weight = 1.0f / (d2 + 10.0f);

                sumBase   += focuses[f].base * weight;
                sumScale  += focuses[f].stepScale * weight;
                sumOffset += focuses[f].stepOffset * weight;
                totalWeight += weight;
            }

            pixelBase[idx]       = sumBase / totalWeight;
            pixelStepScale[idx]  = sumScale / totalWeight;
            pixelStepOffset[idx] = sumOffset / totalWeight;

            currentGrid[idx] = randomf(-1.0f, 1.0f);
            bestVariation[idx] = 1e38;
        }
    }

    levels = 9;
    blurFactor = 0.7f;

}

void setup() {

    srand(read_cycle());

    sysctl_set_spi0_dvp_data(1);
    dmac_init();

    lcd_init(SPI0, 3, 6, 7, 20000000, 37, 38, 3);
    lcd_clear(0xFFFF);

    gridA           = (float*)malloc(SCR * sizeof(float));
    gridB           = (float*)malloc(SCR * sizeof(float));
    blurBuffer      = (float*)malloc(SCR * sizeof(float));
    bestVariation   = (float*)malloc(SCR * sizeof(float));
    inhibitor       = (float*)malloc(SCR * sizeof(float));
    bestLevel       = (int*)malloc(SCR * sizeof(int));
    direction       = (bool*)malloc(SCR * sizeof(bool));
    frameBuffer     = (uint16_t*)malloc(SCR * sizeof(uint16_t));

    pixelBase       = (float*)malloc(SCR * sizeof(float));
    pixelStepScale  = (float*)malloc(SCR * sizeof(float));
    pixelStepOffset = (float*)malloc(SCR * sizeof(float));

    currentGrid = gridA;
    nextGrid = gridB;

    rndseed();

}

void loop() {

    float *currentActivator = currentGrid;
    float *currentInhibitor = inhibitor;

    for (int i = 0; i < SCR; i++) {
        bestVariation[i] = 1e38f;
    }

    for (int lvl = 0; lvl < levels - 1; lvl++) {

        float *p_blur = blurBuffer;
        float *p_act = currentActivator;
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                if (y == 0 && x == 0) *p_blur = *p_act;
                else if (y == 0) *p_blur = *(p_blur - 1) + *p_act;
                else if (x == 0) *p_blur = *(p_blur - WIDTH) + *p_act;
                else *p_blur = *(p_blur - 1) + *(p_blur - WIDTH) - *(p_blur - WIDTH - 1) + *p_act;
                p_blur++; p_act++;
            }
        }

        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                int idx = y * WIDTH + x;

                int maxRadius = fminf(WIDTH, HEIGHT) / 3;
                int radius = (int)powf(pixelBase[idx], lvl);
                if (radius < 1) radius = 1;
                if (radius > maxRadius) radius = maxRadius;

                int miny = (y - radius < 0) ? 0 : y - radius;
                int maxy = (y + radius >= HEIGHT) ? HEIGHT - 1 : y + radius;
                int minx = (x - radius < 0) ? 0 : x - radius;
                int maxx = (x + radius >= WIDTH) ? WIDTH - 1 : x + radius;
                float area = (float)((maxx - minx) * (maxy - miny));

                float inh = (blurBuffer[maxy * WIDTH + maxx] - blurBuffer[maxy * WIDTH + minx]
                           - blurBuffer[miny * WIDTH + maxx] + blurBuffer[miny * WIDTH + minx]) / area;

                float v = fabsf(currentActivator[idx] - inh);
                if (lvl == 0 || v < bestVariation[idx]) {
                    bestVariation[idx] = v;
                    bestLevel[idx] = lvl;
                    direction[idx] = currentActivator[idx] > inh;
                }
                currentInhibitor[idx] = inh;
            }
        }

        currentActivator = currentInhibitor;
    }

    float smallest = 1e38;
    float largest = -1e38;

    for (int i = 0; i < SCR; i++) {
        int radiusAtBestLevel = (int)powf(pixelBase[i], bestLevel[i]);
        if (radiusAtBestLevel < 1) radiusAtBestLevel = 1;

        float move = logf((float)radiusAtBestLevel) * pixelStepScale[i] + pixelStepOffset[i];

        if (direction[i]) nextGrid[i] = currentGrid[i] + move;
        else nextGrid[i] = currentGrid[i] - move;

        if (nextGrid[i] < smallest) smallest = nextGrid[i];
        if (nextGrid[i] > largest) largest = nextGrid[i];
    }

    float range = (largest - smallest) * 0.5f;

    for (int i = 0; i < SCR; i++) { 

        nextGrid[i] = ((nextGrid[i] - smallest) / range) - 1.0f;
        float valF = 128.0f + (127.0f * nextGrid[i]);
        frameBuffer[i] = grayLUT[(uint8_t)valF];

    }

    float *temp = currentGrid;
    currentGrid = nextGrid;
    nextGrid = temp;

    lcd_draw_picture(0, 0, WIDTH, HEIGHT, (uint16_t*)frameBuffer);

}