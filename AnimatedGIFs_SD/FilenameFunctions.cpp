#include "FilenameFunctions.h" //defines USE_SPIFFS as reqd

#include "FS.h"

#if  USE_SD_MMC
#include <SD_MMC.h>
#define SDCARD_CMD 15
#define SDCARD_D0 2
#define SDCARD_D1 4
#define SDCARD_D2 12
#define SDCARD_D3 13
#define FILESYSTEM SD_MMC
#elif USE_SPI_SD
#include <SD.h>
#define FILESYSTEM SD
#elif USE_SPIFFS
#include <SPIFFS.h>
#define FILESYSTEM SPIFFS
#else
#error "Please select filesystem"
#endif


File file;
int numberOfFiles;

bool initSdCard(int cs)
{
#if USE_SD_MMC
    pinMode(SDCARD_CMD, PULLUP); // CMD, needed in 4- and 1- line modes
    pinMode(SDCARD_D0, PULLUP);  // D0, needed in 4- and 1-line modes
    pinMode(SDCARD_D1, PULLUP);  // D1, needed in 4-line mode only
    pinMode(SDCARD_D2, PULLUP);  // D2, needed in 4-line mode only
    pinMode(SDCARD_D3, PULLUP);  // D3, needed in 4- and 1-line modes
    return FILESYSTEM.begin();
#elif USE_SPI_SD
    if (cs == -1)
        return false;
    pinMode(cs, OUTPUT);
    return FILESYSTEM.begin(cs);
#elif USE_SPIFFS
    return FILESYSTEM.begin();
#endif
}


bool fileSeekCallback(unsigned long position)
{
#if USE_SPIFFS
    return file.seek(position, SeekSet);
#else
    return file.seek(position);
#endif
}

unsigned long filePositionCallback(void)
{
    return file.position();
}

int fileReadCallback(void)
{
    return file.read();
}

int fileReadBlockCallback(void *buffer, int numberOfBytes)
{
    return file.read((uint8_t *)buffer, numberOfBytes); 
}




bool isAnimationFile(const char filename[])
{
    if (filename[0] == '_')
        return false;

    if (filename[0] == '~')
        return false;

    if (filename[0] == '.')
        return false;

    String filenameString = String(filename); //.kbv STM32 and ESP need separate statements
    filenameString.toUpperCase();
    if (filenameString.endsWith(".GIF") != 1)
        return false;

    return true;
}

// Enumerate and possibly display the animated GIF filenames in GIFS directory
int enumerateGIFFiles(const char *directoryName, bool displayFilenames)
{
    char *filename;
    numberOfFiles = 0;

#ifdef USE_SPIFFS_DIR
    File file;
    Dir directory = FILESYSTEM.openDir(directoryName);
    while (directory.next())
    {
        file = directory.openFile("r");
        if (!file)
            break;
#else
    File file;
    Serial.println("Open dir : " + String(directoryName));
    File directory = FILESYSTEM.open(directoryName);
    if (!directory)
    {
        Serial.println("Open fail");
        return -1;
    }

    while (file = directory.openNextFile())
    {
#endif
        filename = (char *)file.name();
        if (isAnimationFile(filename))
        {
            numberOfFiles++;
            if (displayFilenames)
            {
                Serial.print(numberOfFiles);
                Serial.print(":");
                Serial.print(filename);
                Serial.print("    size:");
                Serial.println(file.size());
            }
        }
        else
            Serial.println(filename);
        file.close();
    }

    //    file.close();
#ifdef USE_SPIFFS
#else
    directory.close();
#endif

    return numberOfFiles;
}

// Get the full path/filename of the GIF file with specified index
void getGIFFilenameByIndex(const char *directoryName, int index, char *pnBuffer)
{

    char *filename;

    // Make sure index is in range
    if ((index < 0) || (index >= numberOfFiles))
        return;

#ifdef USE_SPIFFS_DIR
    Dir directory = FILESYSTEM.openDir(directoryName);
    //    if (!directory) return;

    while (directory.next() && (index >= 0))
    {
        file = directory.openFile("r");
        if (!file)
            break;
#else
    File directory = FILESYSTEM.open(directoryName);
    if (!directory)
    {
        return;
    }

    while ((index >= 0))
    {
        file = directory.openNextFile();
        if (!file)
            break;
#endif

        filename = (char *)file.name(); //.kbv
        if (isAnimationFile(filename))
        {
            index--;

            // Copy the directory name into the pathname buffer
            strcpy(pnBuffer, directoryName);
#if defined(ESP32) || defined(USE_SPIFFS)
            pnBuffer[0] = 0;
#else
            int len = strlen(pnBuffer);
            if (len == 0 || pnBuffer[len - 1] != '/')
                strcat(pnBuffer, "/");
#endif
            // Append the filename to the pathname
            strcat(pnBuffer, filename);
        }

        file.close();
    }

    file.close();
#ifdef USE_SPIFFS
#else
    directory.close();
#endif
}

int openGifFilenameByIndex(const char *directoryName, int index)
{
    char pathname[40];

    getGIFFilenameByIndex(directoryName, index, pathname);

    Serial.print("Pathname: ");
    Serial.println(pathname);

    if (file)
        file.close();

        // Attempt to open the file for reading
#ifdef USE_SPIFFS
    file = FILESYSTEM.open(pathname, "r");
#else
    file = FILESYSTEM.open(pathname);
#endif
    if (!file)
    {
        Serial.println("Error opening GIF file");
        return -1;
    }

    return 0;
}

// Return a random animated gif path/filename from the specified directory
void chooseRandomGIFFilename(const char *directoryName, char *pnBuffer)
{

    int index = random(numberOfFiles);
    getGIFFilenameByIndex(directoryName, index, pnBuffer);
}
