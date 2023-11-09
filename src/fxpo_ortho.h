#ifndef FXPO_ORTHO_H
#define FXPO_ORTHO_H

#include "fxpo_common.h"

#define CHUNK_SIZE           256
#define CHUNKS_PER_TILE_SIDE 16
#define CHUNKS_PER_TILE      CHUNKS_PER_TILE_SIDE * CHUNKS_PER_TILE_SIDE
#define TILE_WIDTH           CHUNK_SIZE * CHUNKS_PER_TILE_SIDE
#define TILE_HEIGHT          TILE_WIDTH
#define TILE_SIZE            TILE_WIDTH * TILE_HEIGHT

#define MAX_ZOOM_LEVEL     22
#define MAX_QUADKEY_LENGTH MAX_ZOOM_LEVEL
#define MAX_URL_LENGTH     255

enum fxpo_provider {
  FXPO_PROVIDER_BI,
  FXPO_PROVIDER_COUNT
};

struct fxpo_tile_t {
  uint32_t x;
  uint32_t y;
  uint8_t  zoom_level;
  enum fxpo_provider provider;
};

struct fxpo_chunk_t {
  uint32_t x;
  uint32_t y;
  uint8_t  zoom_level;
  bool     found;
};

/* fxpo_ortho_tile2quadkey converts a tile x, y coordiate to a Bing maps quadkey.
   quadkey must be initialised and must not be longer than FXPO_MAX_QUADKEY_SIZE. */
void
fxpo_ortho_tile2quadkey( uint32_t x,
                         uint32_t y,
                         uint8_t  zoom_level,
                         char *   quadkey );

/* fxpo_ortho_wgs2tile converts a World Geodetic System 1984 (WGS84) coordinate to
   an orthotile at the specified zoom level. */
void
fxpo_ortho_wgs2tile( double               lat,
                     double               lon,
                     uint8_t              zoom_level,
                     struct fxpo_tile_t * tile );

void
fxpo_ortho_downsample_chunk( struct fxpo_chunk_t * chunk );

void
fxpo_ortho_chunk_bbox( uint32_t   downsampled_chunk_x,
                       uint32_t   downsampled_chunk_y,
                       uint32_t   chunk_x,
                       uint32_t   chunk_y,
                       uint8_t    downsample,
                       uint32_t * x,
                       uint32_t * y,
                       uint32_t * w,
                       uint32_t * h );

/* fxpo_ortho_build_url builds an HTTP URL to fetch the image associated with a chunk. */
enum fxpo_status
fxpo_ortho_build_url( enum fxpo_provider provider,
                      const char *       quadkey,
                      char *             url,
                      size_t             url_len );

void
fxpo_ortho_build_dds_path( const char *               scenery_path,
                           const char *               tileset,
                           const struct fxpo_tile_t * tile,
                           char *                     path,
                           size_t                     path_len );

size_t
fxpo_ortho_find_tiles( const char *           scenery_path,
                       const char *           tileset,
                       struct fxpo_tile_t *** tiles );

#endif