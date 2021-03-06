/*
*
* STM32_GPU Project
* Creation start: 15.07.2016
*
* Created by: Antonov Alexandr (Bismuth208)
*
*/

#include <string.h>

#include <gfx.h>
#include <ff.h>
#include <diskio.h>
#include <sdcard_spi.h>
#if USE_FSMC
 #include <fsmcdrv.h>
#else
 #include <spi.h>
#endif

#include "sdLoader.h"
#include "tiles.h"

#define LCD_BUFFER_SIZE  80     // for rgb565 color for LCD
#define RGB_BUFFER_SIZE  240    // for r,g,b pixels
#define MAX_SD_INIT_ATTEMPTS 30 // yep, sometimes need so much attempts...

#define LCD_BMP16_LINE_SIZE     (LCD_BUFFER_SIZE*2)
#define LCD_BMP16_1_2LINE_SIZE  (LCD_BUFFER_SIZE/2)

//===========================================================================//

uint8_t  sdbuffer[RGB_BUFFER_SIZE];  // pixel in buffer (R+G+B per pixel)
uint16_t lcdbuffer[LCD_BUFFER_SIZE]; // pixel out buffer (16-bit per pixel)

//uint8_t buffer[50]; // for loaded text
//===========================================================================//

FATFS SDfs;				/* File system object for each logical drive */
FIL File;				/* File objects */
FRESULT result;

//===========================================================================//

void init_sdCard(void)
{
  print( T_MOUNT_SD );
  
  int8_t attempts = MAX_SD_INIT_ATTEMPTS; // how many times try to mount drive
  
  do {
    // mount drive immedetly
    result = f_mount(&SDfs, "0", 1);
    
    --attempts;
    if(attempts%3) { // make a little less dots..
      print("."); // show what init is in progress
    }
  } while((result != FR_OK) && (attempts)); // have attempts and not mounted
  
  if(result == FR_OK) {
    print(T_OK);
    
  } else {
    print(T_FAIL);
  }
}


FRESULT openSDFile(void *pFileName, const char *extension)
{
  strcat( (char*)pFileName, extension );
  result = f_open(&File, (char*)pFileName, FA_OPEN_EXISTING | FA_READ);
  
  return result;
}


// ---------------------- Tiles 8,16,32 -------------------------------- //
/*
 * is it possible to remove a half of that?
 * make loading tiles faster and smaller?
 * maybe make simpler math of pointing single tile?
 *
 * code below something like mind blow...
 * and it looks like a f@@@ing magic!
 * how i make it in the past!?
 */


pfGetFunc_t getFuncArrTilePointer(uint16_t tleBaseSize)
{
  pfGetFunc_t pTmpFunc =0;
  
  switch(tleBaseSize)
  {
  case TILE_8_BASE_SIZE: {
    pTmpFunc = getArrTilePointer8x8;
  } break;
  
  case TILE_16_BASE_SIZE: {
    pTmpFunc = getArrTilePointer16x16;
  } break;
  
#if defined(STM32F10X_HD) || defined(STM32F40XX)
  case TILE_32_BASE_SIZE: {
    pTmpFunc = getArrTilePointer32x32;
  } break;
#endif
  }
  
  return pTmpFunc;
}

void SDLoadTile(void *fileName, uint8_t *pData, uint16_t tleBaseSize, uint16_t tlesNumSize)
{
  /*
  * This function load single Tile from Tileset located in SD card
  * and store loaded data to RAM
  *
  * fileName        - name of tile set array on SD card
  * tileSetW        - width in NUMBER of tiles in tile set
  * ramTileBase     - tile number in ram, where to store loaded tile
  * tileMin         - number of tile in tileset which must me loaded from SD card
  */
  
  UINT cnt;
  uint16_t tleOffsetX = 0;
  uint16_t tleNewLineOffsetX = 0;
  
  tileParam_t *tilePar = (tileParam_t*)pData;
  
  // little RAM protection
  if(tilePar->ramTileBase > tlesNumSize) return;
  
  pfGetFunc_t pfGetArrTilePointer = getFuncArrTilePointer(tleBaseSize);
  uint8_t *pTileArr = pfGetArrTilePointer(tilePar->ramTileBase);
  
  if(openSDFile(fileName, T_TLE_SET_EXT_NAME) == FR_OK) {
    
    tleNewLineOffsetX = ((tilePar->tileSetW * tleBaseSize) - tleBaseSize);
    
    // calculate x offset for start of scanline
    tleOffsetX = tilePar->tileMin * tleBaseSize;
    // set start of scanline; 2 - because tile is square
    tilePar->tileMin > tilePar->tileSetW ? f_lseek(&File, tleOffsetX*2) : f_lseek(&File, tleOffsetX);
    
    for(uint8_t tileYcnt=0; tileYcnt < tleBaseSize; tileYcnt++) {
      f_read(&File, pTileArr, tleBaseSize, &cnt); // read single scanline
      f_lseek(&File, f_tell(&File) + tleNewLineOffsetX); // make new scanline
      pTileArr += tleBaseSize;
    }
  } 
  f_close(&File);
  // TODO: add some error code when return
}

void SDLoadTileSet(void *fileName, uint8_t *pData, uint16_t tleBaseSize, uint16_t tlesNumSize)
{
  /*
  * This function load selected region of tiles from Tileset located in SD card.
  * Tiles load from tileMin position, to tileMax position;
  * Store loaded data to RAM from ramTileBase to (ramTileBase + tileMax);
  *
  * fileName        - name of tileset array on SD card
  * tileSetW        - width in NUMBER of tiles in tile set
  * ramTileBase     - base tile number in ram, where to start store loaded tiles
  * tileMin         - base tile number in tileset where loading start
  * tileMax         - number of tiles in tileset which must be loaded from SD card
  */
  
  UINT cnt;
  uint8_t tileYcnt;
  uint16_t tleOffsetX = 0;
  uint16_t tleNewLineOffsetX = 0;
  
  uint8_t *pTileArr =0;
  tileParam_t *tilePar = (tileParam_t*)pData;
  
  // little RAM protection
  if((tilePar->ramTileBase + tilePar->tileMax) > tlesNumSize) return;
  
  pfGetFunc_t pfGetArrTilePointer = getFuncArrTilePointer(tleBaseSize);
  
  if(openSDFile(fileName, T_TLE_SET_EXT_NAME) == FR_OK) {
    
    tleNewLineOffsetX = tilePar->tileSetW * tleBaseSize - tleBaseSize;

    for(uint8_t tileNum=tilePar->tileMin; tileNum < tilePar->tileMax; tileNum++) {
      
      // calculate x offset for start of scanline
      tleOffsetX = tileNum * tleBaseSize;
      // set start of scanline; 2 - because tile is square
      tileNum > tilePar->tileSetW ? f_lseek(&File, tleOffsetX*2) : f_lseek(&File, tleOffsetX);
      
      // get pointer to new tile in RAM
      pTileArr = pfGetArrTilePointer(tilePar->ramTileBase + tileNum);

      // load single tile to RAM
      for(tileYcnt = 0; tileYcnt < tleBaseSize; tileYcnt++) { // Y
        
        f_read(&File, pTileArr, tleBaseSize, &cnt); // read single scanline
        f_lseek(&File, f_tell(&File) + tleNewLineOffsetX); // make new scanline
        pTileArr += tleBaseSize;
      }
    }
  }
  f_close(&File);
  // TODO: add some error code when return
}
// --------------------------------------------------------------------- //


// --------------------- Tile Map -------------------------- //
void SDLoadTileMap(void *fileName)
{
  if(openSDFile(fileName, T_MAP_SET_EXT_NAME) == FR_OK) {
    
    UINT cnt;
    uint8_t *pTileMapArr = getMapArrPointer();
    
    for(uint8_t countH=0; countH < BACKGROUND_SIZE_H; countH++) {
      f_read(&File, pTileMapArr, BACKGROUND_SIZE_W, &cnt);
      pTileMapArr += BACKGROUND_SIZE_W;
    }
  } else {
    // TODO: add some error code when return
    return;
  }
  
  f_close(&File);
}

// slow but not need additionnal program to convert tile map from 'pixelEdit'
// aaand much much flexible...
// just rename *.txt file to *.map, and add {} at begin and end of data array
#if 0
void SDLoadTileMap(void *fileName)
{
  UINT cnt;
  bool enterArray = false;
  bool endArray = false;
  uint8_t enterChar = 0;
  uint8_t tileId = 0;
  uint16_t tileIdCount =0;
  
  uint8_t *pTileMapArr = getMapArrPointer();
  
  if(openSDFile(fileName, T_MAP_SET_EXT_NAME) == FR_OK) {
    
    while(!enterArray) {
      f_read(&File, (void*)enterChar, 1, &cnt);
      
      if((enterChar == '{') || (enterChar == '[')) {
        enterArray = true;
      }
    }
    
    enterArray = false;
    
    while((f_tell(&File) != f_size(&File)) && (!endArray)) {
      
      tileId = 0;
      
      while(!enterArray) {
        f_read(&File, (void*)enterChar, 1, &cnt);
        
        if((enterChar == ',') || (enterChar == LF_FILE_MARK) || (enterChar == CR_FILE_MARK)) {
          enterArray = true;
        } else if((enterChar == '}') || (enterChar == ']')) {
          endArray = false;
          enterArray = true;
        } else {
          tileId = ((tileId*10) | (enterChar-0x30)); // convert ASCII to number...
          pTileMapArr[tileIdCount] = tileId;
          ++tileIdCount;
        }
      }
    }
  }
  
  f_close(&File);
}
#endif
// --------------------------------------------------------- //


// --------------------- Palette --------------------------- //
void SDLoadPalette(void *fileName)
{
  if(openSDFile(fileName, T_PAL_SET_EXT_NAME) == FR_OK) {
    UINT cnt;
    uint8_t  r, g, b;
    uint16_t colors = f_size(&File) /3;
    
    if(colors <= 255) {
      f_read(&File, sdbuffer, f_size(&File), &cnt);
      
      for(uint16_t count =0; count < colors; count++) {
        r = sdbuffer[3 * count];
        g = sdbuffer[3 * count + 1];
        b = sdbuffer[3 * count + 2];
        currentPaletteArr[count] = convRGBto565M(r, g, b);
      } 
    }
    
    palChanged = 1; // force
    //tleForeRedraw.allTle = 0x00; // 0 is need to redraw
    
  }
  f_close(&File);
  // TODO: add some error code when return
}
// --------------------------------------------------------- //



// --------------------------------------------------------- //
#if 0
void loadExecCSVFile(void *fileName)
{
  if(openSDFile(fileName, T_CSV_SET_EXT_NAME) == FR_OK) {
    
  }
  
  f_close(&File);
  // TODO: add some error code when return
}
#endif


// ---------------- *.BMP pictures ------------------------- //
// These read 16- and 32-bit types from the SD card file.
uint16_t read16()
{
  UINT cnt;
  uint16_t result16;
  
  f_read(&File, (uint8_t*)&result16, 2, &cnt);
  
  return result16;
}

uint32_t read32()
{
  UINT cnt;
  uint32_t result32;

  f_read(&File, (uint8_t*)&result32, 4, &cnt);
  
  return result32;
}

// i'll fix problem whith 'w' later
void drawBMP24(uint16_t w, uint16_t h)
{
  UINT cnt; // how much bytes really read
  uint8_t  r, g, b;

  for(uint16_t row = 0; row < h; row++) { // For each scanline...
    for(uint16_t col = 0; col < w; col += LCD_BUFFER_SIZE) {
      // read pixels into the buffer
      f_read(&File, sdbuffer, RGB_BUFFER_SIZE, &cnt);
      
      // convert color
      for(uint16_t p = 0; p < LCD_BUFFER_SIZE; p++) {
        b = sdbuffer[3 * p];
        g = sdbuffer[3 * p + 1];
        r = sdbuffer[3 * p + 2];
        lcdbuffer[p] = convRGBto565M(r, g, b);
      }
      // push buffer to TFT
      SEND_ARR16_FAST(lcdbuffer, LCD_BUFFER_SIZE);
    }
  }
}

// i'll fix problem whith 'w' later
void drawBMP16(uint16_t w, uint16_t h)
{
  UINT cnt;
  
  for(uint16_t row = 0; row < h; row++) { // For each scanline...
    for(uint16_t col = 0; col < w; col += LCD_BUFFER_SIZE) {
#if defined(STM32F10X_MD) || defined(STM32F10X_HD)
      // little protection...
      while(DMA1_Channel3->CNDTR > LCD_BMP16_1_2LINE_SIZE);
#else
      // TODO: add DMA_Channel_3
#endif
      // read pixels into the buffer
      f_read(&File, lcdbuffer, LCD_BMP16_LINE_SIZE, &cnt);
      
      // push buffer to TFT
      SEND_ARR16_FAST(lcdbuffer, LCD_BUFFER_SIZE);
    }
  }
}

void SDPrintBMP(uint16_t x, uint16_t y, void *fileName)
{
  UINT cnt;
  
  int32_t  bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth; must be 24 or 16
  uint16_t w, h;
  
  bmpCore_t bmpHeader; // base header info
  
  if((x >= width()) || (y >= height())) return;
  
  if(openSDFile(fileName, T_BMP_SET_EXT_NAME) == FR_OK) {
    // read core header info
    f_read(&File, bmpHeader.bmpArr, BMP_FILE_HEADER_SIZE, &cnt);
    
    // Parse BMP header
    if(bmpHeader.fileMark == BMP_FILE_MARK) { // BMP signature; 0x00
      
      if(bmpHeader.headerSize > BMP_345_VER_MARK) { // 3, 4, and 5 version
        bmpWidth = read32();   // 0x12
        bmpHeight = read32();  // 0x16
      } else { // CORE version
        bmpWidth = read16();   // 0x12
        bmpHeight = read16();  // 0x14
      }
      
      if(read16() == 1) {    // must be '1'; 0x16 or 0x1A
        bmpDepth = read16(); // bits per pixel; 0x18 or 0x1C
        
        //(void)read32(); // get compression info 0 = uncompressed, 1 = comressed
        f_lseek(&File, bmpHeader.bmpImageoffset); // skip header
        
        // Crop area to be loaded
        w = bmpWidth;
        h = bmpHeight;
        if ((x + w - 1) >= width())  w = width() - x;
        if ((y + h - 1) >= height()) h = height() - y;
        
        // Set TFT address window to clipped image bounds
        setAddrWindow(x, y, x + w - 1, y + h - 1);
        
        switch(bmpDepth)
        {
        case 16: { // 565 format
          drawBMP16(w, h);
        } break;
        
        case 24: { // standard 24bit bmp
          drawBMP24(w, h);
        } break;
        
        default: break; //print("Unsupported Bit Depth.");
        }
      }
    }
  }
  f_close(&File);
}
// --------------------------------------------------------- //
