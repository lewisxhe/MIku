#include <SPI.h>
#include "GifDecoder.h"
#include "FilenameFunctions.h" //defines USE_SPIFFS
#include "miku128x1281.h"
#include "miku320.h"

#define M0(x)            \
    {                    \
        x, #x, sizeof(x) \
    }
// #define USE_ESPI

#define DISPLAY_TIME_SECONDS 80
#define GIF_DIRECTORY "/gifs"

#ifdef USE_ESPI
#include <TFT_eSPI.h>
#define SCREEN_WIDTH 128//320
#define SCREEN_HEIGHT 128//240
#define SCREEN_MISO 12
#define SCREEN_MOSI 23
#define SCREEN_SCLK 18
#define SCREEN_CS 16//27 // Chip select control pin
#define SCREEN_DC 17//26 // Data Command control pin
#define SCREEN_RST 5 // Reset pin (could connect to RST pin)
TFT_eSPI tft = TFT_eSPI();
#else
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 128
#define SCREEN_MOSI 23
#define SCREEN_SCLK 18
#define SCREEN_DC 4
#define SCREEN_CS_0 16
#define SCREEN_CS_1 17
#define SCREEN_RST 2
typedef Adafruit_SSD1351 screen_t;
#endif

#define SCREEN_NUM (sizeof(screen) / sizeof(screen[0]))

SPISettings settings(16000000, MSBFIRST, SPI_MODE0); // 26.667MHz seems reliable on the ESP32.

#ifndef USE_ESPI
struct
{
    screen_t tft;
    int cs;
} screen[] = {
    screen_t(SCREEN_CS_0, SCREEN_DC, 0),
    SCREEN_CS_0,
    screen_t(SCREEN_CS_1, SCREEN_DC, 0),
    SCREEN_CS_1,
};
#endif

/*  template parameters are maxGifWidth, maxGifHeight, lzwMaxBits

    The lzwMaxBits value of 12 supports all GIFs, but uses 16kB RAM
    lzwMaxBits can be set to 10 or 11 for small displays, 12 for large displays
    All 32x32-pixel GIFs tested work with 11, most work with 10
*/
GifDecoder<128, 128, 12> decoder;

int num_files;

typedef struct
{
    const unsigned char *data;
    const char *name;
    uint32_t sz;
} gif_detail_t;

gif_detail_t gifs[] = {
    // M0(miku320),
    M0(miku128x1281),
};

const uint8_t *g_gif;
uint32_t g_seek;
int width, height, endX, endY;
uint16_t pBuffer[128 * 128 + 128];
size_t n = 0;

bool fileSeekCallback_P(unsigned long position)
{
    g_seek = position;
    return true;
}

unsigned long filePositionCallback_P(void)
{
    return g_seek;
}

int fileReadCallback_P(void)
{
    return pgm_read_byte(g_gif + g_seek++);
}

int fileReadBlockCallback_P(void *buffer, int numberOfBytes)
{
    memcpy_P(buffer, g_gif + g_seek, numberOfBytes);
    g_seek += numberOfBytes;
    return numberOfBytes; //.kbv
}

void screenClearCallback(void)
{
}

bool openGifFilenameByIndex_P(const char *dirname, int index)
{
    gif_detail_t *g = &gifs[index];
    g_gif = g->data;
    g_seek = 0;
    Serial.print("Flash: ");
    Serial.print(g->name);
    Serial.print(" size: ");
    Serial.println(g->sz);
    return index < num_files;
}

void drawPixelCallback(int16_t x, int16_t y, uint8_t red, uint8_t green, uint8_t blue)
{
}

void updateScreenCallback(void)
{
    n = 0;

    SPI.beginTransaction(settings);

#ifdef USE_ESPI
    tft.setWindow(0, 0, endX, endY);
    digitalWrite(SCREEN_CS, LOW); // Chip select
    digitalWrite(SCREEN_DC, HIGH);   // Data mode
    SPI.writePixels((uint8_t *)pBuffer, height * width * 2);
    digitalWrite(SCREEN_CS, HIGH); // Deselect
#else
    for (int i = 0; i < SCREEN_NUM; i++)
    {

        screen[i].tft.writeCommand(SSD1351_CMD_SETROW); // Y range
                                                        // screen[i].tft.writeData(SCREEN_HEIGHT - 1);
        screen[i].tft.writeData(0);
        screen[i].tft.writeData(endY);

        screen[i].tft.writeCommand(SSD1351_CMD_SETCOLUMN); // X range
                                                           // screen[i].tft.writeData(SCREEN_WIDTH - 1);
        screen[i].tft.writeData(0);
        screen[i].tft.writeData(endX);
        screen[i].tft.writeCommand(SSD1351_CMD_WRITERAM); // Begin write

        digitalWrite(screen[i].cs, LOW); // Chip select
        digitalWrite(SCREEN_DC, HIGH);   // Data mode

        if (height > SCREEN_HEIGHT)
            height = SCREEN_HEIGHT;
        if (width > SCREEN_WIDTH)
            width = SCREEN_WIDTH;
        SPI.writePixels((uint8_t *)pBuffer, height * width * 2);

        digitalWrite(screen[i].cs, HIGH); // Deselect
    }
#endif
    SPI.endTransaction();
}

void drawLineCallback(int16_t x, int16_t y, uint8_t *buf, int16_t w, uint16_t *palette, int16_t skip)
{
    uint8_t pixel;
    if (y >= SCREEN_HEIGHT || x >= SCREEN_WIDTH)
        return;
    if (x + w > SCREEN_WIDTH)
        w = SCREEN_WIDTH - x;
    if (w <= 0)
        return;
    endX = x + w - 1;
    endY = y;
    for (int i = 0; i < w;)
    {
        pixel = buf[i++];
        if (pixel == skip)
            break;
        if (n >= SCREEN_WIDTH * SCREEN_HEIGHT)
        {
            return;
        }
        pBuffer[n++] = palette[pixel];
    }
}

void setup()
{
    Serial.begin(115200);

#ifdef USE_ESPI
    tft.init();
    pinMode(SCREEN_CS, OUTPUT);
    pinMode(SCREEN_DC, OUTPUT);
    tft.fillScreen(0);
#else
    pinMode(SCREEN_RST, OUTPUT);
    digitalWrite(SCREEN_RST, LOW);
    delay(10);
    digitalWrite(SCREEN_RST, HIGH);
    delay(50);

    // Deselect all
    for (int i = 0; i < SCREEN_NUM; i++)
    {
        pinMode(screen[i].cs, OUTPUT);
        digitalWrite(screen[i].cs, HIGH);
    }

    for (int i = 0; i < SCREEN_NUM; i++)
    {
        digitalWrite(screen[i].cs, LOW);
        screen[i].tft.begin();
        screen[i].tft.setRotation(0);
        digitalWrite(screen[i].cs, HIGH);
        screen[i].tft.fillScreen(0);
    }

    screen[0].tft.writeCommand(SSD1351_CMD_SETREMAP);
    screen[0].tft.writeData(0x76);
#endif
    // #ifdef USE_SPIRAM
    //     pBuffer = (uint16_t *)heap_caps_malloc(SCREEN_WIDTH * SCREEN_HEIGHT, MALLOC_CAP_SPIRAM);
    // #else
    //     pBuffer = (uint16_t *)malloc(SCREEN_WIDTH * SCREEN_HEIGHT);
    // #endif
    //     if (!pBuffer)
    //     {
    //         Serial.println("malloc FAIL");
    //         return;
    //     }

    decoder.setDrawPixelCallback(drawPixelCallback);
    decoder.setScreenClearCallback(screenClearCallback);
    decoder.setUpdateScreenCallback(updateScreenCallback);
    decoder.setDrawLineCallback(drawLineCallback);

    // decoder.setFileSeekCallback(fileSeekCallback);
    // decoder.setFilePositionCallback(filePositionCallback);
    // decoder.setFileReadCallback(fileReadCallback);
    // decoder.setFileReadBlockCallback(fileReadBlockCallback);

    // if (!initSdCard())
    // {
    // Serial.println("No SD card");
    decoder.setFileSeekCallback(fileSeekCallback_P);
    decoder.setFilePositionCallback(filePositionCallback_P);
    decoder.setFileReadCallback(fileReadCallback_P);
    decoder.setFileReadBlockCallback(fileReadBlockCallback_P);
    g_gif = gifs[0].data;
    for (num_files = 0; num_files < sizeof(gifs) / sizeof(*gifs); num_files++)
    {
        Serial.println(gifs[num_files].name);
    }
    // }
    // else
    // {
    //     num_files = enumerateGIFFiles(GIF_DIRECTORY, true);
    // }

    // Determine how many animated GIF files exist
    Serial.print("Animated GIF files Found: ");
    Serial.println(num_files);

    if (num_files < 0)
    {
        Serial.println("No gifs directory");
        while (1)
            ;
    }

    if (!num_files)
    {
        Serial.println("Empty gifs directory");
        while (1)
            ;
    }
}

void loop()
{
    static unsigned long futureTime, cycle_start;
    static int index = -1;

    if (futureTime < millis() || decoder.getCycleNo() > 1)
    {
        char buf[80];
        int32_t now = millis();
        int32_t frames = decoder.getFrameCount();
        int32_t cycle_design = decoder.getCycleTime();
        int32_t cycle_time = now - cycle_start;
        int32_t percent = (100 * cycle_design) / cycle_time;
        sprintf(buf, "[%ld frames = %ldms] actual: %ldms speed: %d%%",
                frames, cycle_design, cycle_time, percent);
        Serial.println(buf);
        cycle_start = now;
        if (++index >= num_files)
        {
            index = 0;
        }

        int good;
        if (g_gif)
            good = (openGifFilenameByIndex_P(GIF_DIRECTORY, index) >= 0);
        else
            ;
        // good = (openGifFilenameByIndex(GIF_DIRECTORY, index) >= 0);
        if (good >= 0)
        {
            // tft.fillScreen(g_gif ? MAGENTA : DISKCOLOUR);
            // tft.fillRect(GIFWIDTH, 0, 1, tft.height(), WHITE);
            // tft.fillRect(278, 0, 1, tft.height(), WHITE);

            decoder.startDecoding();
            decoder.getImagesSize(&width, &height);
            Serial.printf("width:%d height:%d\n", width, height);
            // Calculate time in the future to terminate animation
            futureTime = millis() + (DISPLAY_TIME_SECONDS * 1000);
        }
    }

    decoder.decodeFrame();
}

/*
    Animated GIFs Display Code for SmartMatrix and 32x32 RGB LED Panels

    Uses SmartMatrix Library for Teensy 3.1 written by Louis Beaudoin at pixelmatix.com

    Written by: Craig A. Lindley

    Copyright (c) 2014 Craig A. Lindley
    Refactoring by Louis Beaudoin (Pixelmatix)

    Permission is hereby granted, free of charge, to any person obtaining a copy of
    this software and associated documentation files (the "Software"), to deal in
    the Software without restriction, including without limitation the rights to
    use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
    the Software, and to permit persons to whom the Software is furnished to do so,
    subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
    FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
    COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
    IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
    CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/*
    This example displays 32x32 GIF animations loaded from a SD Card connected to the Teensy 3.1
    The GIFs can be up to 32 pixels in width and height.
    This code has been tested with 32x32 pixel and 16x16 pixel GIFs, but is optimized for 32x32 pixel GIFs.

    Wiring is on the default Teensy 3.1 SPI pins, and chip select can be on any GPIO,
    set by defining SD_CS in the code below
    Function     | Pin
    DOUT         |  11
    DIN          |  12
    CLK          |  13
    CS (default) |  15

    This code first looks for .gif files in the /gifs/ directory
    (customize below with the GIF_DIRECTORY definition) then plays random GIFs in the directory,
    looping each GIF for DISPLAY_TIME_SECONDS

    This example is meant to give you an idea of how to add GIF playback to your own sketch.
    For a project that adds GIF playback with other features, take a look at
    Light Appliance and Aurora:
    https://github.com/CraigLindley/LightAppliance
    https://github.com/pixelmatix/aurora

    If you find any GIFs that won't play properly, please attach them to a new
    Issue post in the GitHub repo here:
    https://github.com/pixelmatix/AnimatedGIFs/issues
*/

/*
    CONFIGURATION:
    - If you're using SmartLED Shield V4 (or above), uncomment the line that includes <SmartMatrixShieldV4.h>
    - update the "SmartMatrix configuration and memory allocation" section to match the width and height and other configuration of your display
    - Note for 128x32 and 64x64 displays with Teensy 3.2 - need to reduce RAM:
      set kRefreshDepth=24 and kDmaBufferRows=2 or set USB Type: "None" in Arduino,
      decrease refreshRate in setup() to 90 or lower to get good an accurate GIF frame rate
    - Set the chip select pin for your board.  On Teensy 3.5/3.6, the onboard microSD CS pin is "BUILTIN_SDCARD"
*/
