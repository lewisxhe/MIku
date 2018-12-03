#ifndef FILENAME_FUNCTIONS_H
#define FILENAME_FUNCTIONS_H

#include <Arduino.h>

#define USE_SD_MMC  1
#define USE_SPIFFS  0
#define USE_SPI_SD  0

// #define USE_SPIFFS_DIR


int enumerateGIFFiles(const char *directoryName, bool displayFilenames);
void getGIFFilenameByIndex(const char *directoryName, int index, char *pnBuffer);
int openGifFilenameByIndex(const char *directoryName, int index);
bool initSdCard(int cs = -1);

bool fileSeekCallback(unsigned long position);
unsigned long filePositionCallback(void);
int fileReadCallback(void);
int fileReadBlockCallback(void * buffer, int numberOfBytes);

#endif
