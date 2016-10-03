#ifndef _GPUTTILES_H
#define _GPUTTILES_H

#ifdef __cplusplus
extern "C"{
#endif
  
// -------------------------------------------------------- //
#define TILE_BASE_SIZE          8       // in px

#define TILE_ARR_8X8_SIZE       64
#define TILE_ARR_8X16_SIZE      128
#define TILE_ARR_16X16_SIZE     256
  
#define TILES_NUM_8x8           96
#define TILES_NUM_8x16          16
#define TILES_NUM_16x16         8
  
#define BACKGROUND_SIZE_W       20
#define BACKGROUND_SIZE_H       16
  
#define MAX_TILES_GROUP         20  
  
  
#define TILE_ARR_8X8_SIZE_32       64
// -------------------------------------------------------- //
  
typedef struct {
    uint8_t *pTilesIdArr;
    uint8_t tilesCount;
} tileGroup_t;

typedef struct {
  uint8_t  lasttileNum;          // last tile id in tileArrYxY
  uint8_t  drawed;               // this is problemfix, just left it as is.
  uint16_t *pLastTileArr;       // pointer to last tile array
} lastTile_t;
  
extern const uint16_t *pCurrentPalette;

//extern uint8_t tileArr8x8[TILES_NUM_8x8][TILE_ARR_8X8_SIZE];
//extern uint8_t tileArr8x16[TILES_NUM_8x16][TILE_ARR_8X16_SIZE];
//extern uint8_t tileArr16x16[TILES_NUM_16x16][TILE_ARR_16X16_SIZE];

// -------------------------------------------------------- //

void setCurrentPalette(const uint16_t *newPalette);

uint8_t *getArrTilePointer8x8(uint8_t tileNum);
void addTile8x8(uint8_t tileNum, uint8_t *pTile);


void loadTile8x8(uint8_t tileNum, uint8_t tileSetW, uint8_t *pTile, const uint8_t *pTileSet);
void loadTileSet(uint8_t tileSetSize, uint8_t tileSetW, const uint8_t *pTileSet, uint8_t tileSize);

void drawTile8x8(int16_t posX, int16_t posY, uint8_t tileNum);

void repeatTile8x8(int16_t posX, int16_t posY);


void drawTileMap(int16_t x, int16_t y, const uint8_t *pTileMap, const uint8_t tileSize);


void *memcpy32(void *dst, void const *src, uint32_t len);
// -------------------------------------------------------- //
  
#ifdef __cplusplus
} // extern "C"
#endif

#endif /* _GPUTTILES_H */