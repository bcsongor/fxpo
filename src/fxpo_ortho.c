#include "fxpo_ortho.h"
#include "fxpo_common.h"
#include "fxpo_log.h"
#include "fxpo_alloc.h"

#define INITIAL_CAPACITY 100

#define BI_CACHE_VARIANT_1 "14041" /* Latest. */
#define BI_CACHE_VARIANT_2 "13816" /* Seems older, more green in UK. */

static const char * const FXPO_PROVIDER_STR[] = {
  "BI",
};

static inline const char *
fxpo_provider_str( enum fxpo_provider provider ) {

  if( provider >= FXPO_PROVIDER_BI && provider < FXPO_PROVIDER_COUNT ) return FXPO_PROVIDER_STR[provider];
  return "UNKNOWN";
}

static inline uint32_t
fxpo_pow2( uint32_t pow ) {
  return 1 << pow;
}

void
fxpo_ortho_downsample_chunk( struct fxpo_chunk_t * const chunk ) {

  chunk->x /= 2;
  chunk->y /= 2;
  chunk->zoom_level--;
}

void
fxpo_ortho_chunk_bbox( uint32_t   downsampled_chunk_x,
                       uint32_t   downsampled_chunk_y,
                       uint32_t   chunk_x,
                       uint32_t   chunk_y,
                       uint8_t    downsample,
                       uint32_t * x,
                       uint32_t * y,
                       uint32_t * w,
                       uint32_t * h ) {

  const uint32_t scale = fxpo_pow2( downsample );
  *w = *h = CHUNK_SIZE / scale;
  *x = (chunk_x - scale * downsampled_chunk_x) * (*w);
  *y = (chunk_y - scale * downsampled_chunk_y) * (*h);
}

void
fxpo_ortho_tile2quadkey( uint32_t      x,
                         uint32_t      y,
                         const uint8_t zoom_level,
                         char *        quadkey ) {

  for( uint8_t i = zoom_level; i > 0; i-- ) {
    const uint32_t mask  = fxpo_pow2( i - 1 );
    char           digit = '0';

    if( (x & mask) != 0 ) digit++;
    if( (y & mask) != 0 ) digit += 2;

    *quadkey = digit;
    quadkey++;
  }

  *quadkey = '\0';
}

void
fxpo_ortho_wgs2tile( const double         lat,
                     const double         lon,
                     const uint8_t        zoom_level,
                     struct fxpo_tile_t * tile ) {

  double x       = (lon + 180) / 360;
  double sin_lat = sin( lat * M_PI / 180 );
  double y       = log( (1+sin_lat) / (1-sin_lat) ) / (4*M_PI);

  uint32_t map_size = 256 << zoom_level;

  tile->x = (uint32_t)(x * map_size + 0.5) / 256;
  tile->y = (uint32_t)(y * map_size + 0.5) / 256;
}

enum fxpo_status
fxpo_ortho_build_url( enum fxpo_provider provider,
                      const char *       quadkey,
                      char *             url,
                      size_t             url_len ) {

  static uint8_t server_id = 0;
  #pragma omp threadprivate(server_id)

  switch( provider ) {
    case FXPO_PROVIDER_BI: {
      server_id = server_id % 4 + 1;
      /* Use unencrypted HTTP endpoint to save time on TLS handshake. */
      snprintf( url, url_len, "http://ecn.t%u.tiles.virtualearth.net/tiles/a%s.jpeg?g=" BI_CACHE_VARIANT_1, server_id, quadkey );
      return FXPOS_OK;
    }

    default:
      FXPO_LOG_ERROR( "fxpo_ortho_build_url(): unknown provider %u", provider );
      return FXPOS_INVALID_STATE;
  }
}

void
fxpo_ortho_build_dds_path( const char *               scenery_path,
                           const char *               tileset,
                           const struct fxpo_tile_t * tile,
                           char *                     path,
                           size_t                     path_len ) {

  sprintf_s( path, path_len, "%s/zOrtho4XP_%s/textures/%u_%u_%s%u.dds",
             scenery_path, tileset, tile->y, tile->x, fxpo_provider_str( tile->provider ), tile->zoom_level );
}

inline static void
fxpo_ortho_build_ter_search_path( const char * scenery_path,
                                  const char * tileset,
                                  char *       path,
                                  size_t       path_len ) {

  sprintf_s( path, path_len, "%s/zOrtho4XP_%s/terrain/*.ter", scenery_path, tileset );
}

inline static void
fxpo_ortho_build_ter_path( const char * scenery_path,
                           const char * tileset,
                           const char * file_name,
                           char *       path,
                           size_t       path_len ) {

  sprintf_s( path, path_len, "%s/zOrtho4XP_%s/terrain/%s", scenery_path, tileset, file_name );
}

inline static int
fxpo_qstrcmp( const void * a,
              const void * b ) {
  return strcmp( *(const char **)a, *(const char **)b );
}

size_t
fxpo_ortho_find_tiles( const char *           scenery_path,
                       const char *           tileset,
                       struct fxpo_tile_t *** tiles ) {

  char ** files        = NULL;
  char ** unique_files = NULL;
  size_t  files_len    = 0;
  size_t  tile_count   = 0;

  /*
     List all .ter files in the scenery path.
   */

#ifdef _WIN32
  /* Construct the search path. */
  char path[MAX_PATH_LENGTH];
  fxpo_ortho_build_ter_search_path( scenery_path, tileset, path, sizeof(path) );

  WIN32_FIND_DATA fd;
  HANDLE          h = NULL;

  /* Check if any file are matched. */
  if( ( h = FindFirstFile( path, &fd ) ) == INVALID_HANDLE_VALUE ) {
    FXPO_LOG_ERROR( "fxpo_ortho_find_tiles(): invalid .ter path=%s", path );
    goto cleanup;
  }

  size_t files_capacity = INITIAL_CAPACITY;
  files = fxpo_malloc( files_capacity * sizeof(char *) );

  do {
    if( fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) continue;

    if( files_len == files_capacity ) {
      files_capacity *= 2;
      files           = fxpo_realloc( files, files_capacity * sizeof(char *) );
    }

    files[files_len++] = strdup( fd.cFileName );
  } while( FindNextFile( h, &fd ) );

  FindClose( h );
#else
  /* TODO: Implement POSIX compatible version of the .ter file lookup. */
  FXPO_LOG_ERROR( "fxpo_ortho_find_tiles(): not implemented on this platform" );
  return 0;
#endif

  if( files_len == 0 ) {
    FXPO_LOG_ERROR( "fxpo_ortho_find_tiles(): no .ter files found", scenery_path );
    goto cleanup;
  }

  /*
     Read DDS file name from .ter files.
   */

  const char * key              = "BASE_TEX_NOWRAP";
  const size_t key_len          = strlen( key );
  const char * value_prefix     = "textures/";
  const size_t value_prefix_len = strlen( value_prefix );

  /* Get DDS file names from .ter files. */
  for( size_t i = 0; i < files_len; i++ ) {
    /* Read contents of the .ter file. */
    char ter_path[MAX_PATH_LENGTH];
    fxpo_ortho_build_ter_path( scenery_path, tileset, files[i], ter_path, sizeof(ter_path) );

    FILE * ter = fopen( ter_path, "r" );
    if( ter == NULL ) {
      FXPO_LOG_ERROR( "fxpo_ortho_find_tiles(): could not open file=%s", ter_path );
      goto cleanup;
    }

    char   line[2*MAX_PATH_LENGTH];
    char * value = NULL;

    while( fgets( line, sizeof(line), ter ) ) {
      /* Check if key is BASE_TEX_NOWRAP */
      if( strncmp( line, key, key_len ) == 0 ) {
        /* Extract value of BASE_TEX_NOWRAP which is the expected DDS file path */
        value = line + key_len;
        while( *value == ' ' ) value++;       /* Skip leading whitespaces. */

        value = strstr( value, value_prefix );
        if( value == NULL ) {
          FXPO_LOG_ERROR( "fxpo_ortho_find_tiles(): invalid value for BASE_TEX_NOWRAP line=%s", line );
          fclose( ter );
          goto cleanup;
        }
        value += value_prefix_len;

        value[strcspn( value, "\r\n" )] = '\0';       /* Remove trailing newline. */
        break;
      }
    }

    fclose( ter );

    if( value == NULL ) {
      FXPO_LOG_ERROR( "fxpo_ortho_find_tiles(): could not find BASE_TEX_NOWRAP in file=%s", ter_path );
      goto cleanup;
    }

    /* Replace ter file name with DDS file name. Assumes for every .ter there's a DDS texture. */
    free( files[i] );
    files[i] = strdup( value );
  }

  /*
     Deduplicate DDS file names.
   */

  qsort( files, files_len, sizeof(char *), fxpo_qstrcmp );

  unique_files    = fxpo_malloc( files_len * sizeof(char *) );
  unique_files[0] = files[0];
  size_t unique_files_len = 1;

  for( size_t i = 1; i < files_len; i++ ) {
    if( strcmp( files[i], files[i - 1] ) != 0 ) {
      unique_files[unique_files_len++] = files[i];
    }
  }

  /*
     Construct tiles from DDS file names.
   */

  size_t tiles_capacity = INITIAL_CAPACITY;

  *tiles = fxpo_malloc( tiles_capacity * sizeof(struct fxpo_tile_t *) );

  for( ; tile_count < unique_files_len; tile_count++ ) {
    /* Extract components from file names.
       Example: 63568_40144_BI17.dds */
    const char * const y  = strtok( unique_files[tile_count], "_" );
    const char * const x  = strtok( NULL, "_" );
    const char * const zl = strtok( NULL, "." );

    uint8_t            zoom_level = 0;
    enum fxpo_provider provider;

    /* Iterate over all supported providers. */
    for( provider = 0; provider < FXPO_PROVIDER_COUNT; provider++ ) {
      const char * provider_str     = fxpo_provider_str( provider );
      const size_t provider_str_len = strlen( provider_str );

      if( !strncmp( zl, provider_str, provider_str_len ) ) {
        zoom_level = (uint8_t)strtoul( zl + provider_str_len, NULL, 10 );

        if( zoom_level > MAX_ZOOM_LEVEL ) {
          FXPO_LOG_ERROR( "fxpo_ortho_find_tiles(): tile zl=%u above max_zl=%u", zoom_level, MAX_ZOOM_LEVEL );
          goto cleanup;
        }

        break;
      }
    }

    if( provider == FXPO_PROVIDER_COUNT ) {
      FXPO_LOG_ERROR( "fxpo_ortho_find_tiles(): unknown provider in file=%s", fd.cFileName );
      goto cleanup;
    }

    if( tile_count == tiles_capacity ) {
      /* Double the capacity if the buffer needs to be expanded. */
      tiles_capacity *= 2;
      *tiles          = fxpo_realloc( *tiles, tiles_capacity * sizeof(struct fxpo_tile_t *) );
    }

    struct fxpo_tile_t * const tile = fxpo_malloc( sizeof(struct fxpo_tile_t) );
    tile->x          = strtoul( x, NULL, 10 );
    tile->y          = strtoul( y, NULL, 10 );
    tile->zoom_level = zoom_level;
    tile->provider   = provider;

    (*tiles)[tile_count] = tile;
  }

cleanup:
  if( files != NULL ) {
    for( size_t i = 0; i < files_len; i++ ) free( files[i] );
    free( files );
  }

  if( unique_files != NULL ) free( unique_files );

  if( tile_count == 0 && *tiles != NULL ) {
    free( *tiles );
    *tiles = NULL;
  }

  return tile_count;
}
