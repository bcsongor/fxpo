#include "fxpo_common.h"
#include "fxpo_log.h"
#include "fxpo_alloc.h"
#include "fxpo_http.h"
#include "fxpo_jpeg.h"
#include "fxpo_ortho.h"
#include "fxpo_nvtt3.h"

#pragma warning( push )
#pragma warning( disable : 4067 4456 )
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb/stb_image_resize2.h"
#pragma warning( pop )

void
print_usage( const char * program ) {

  printf( "Usage: %s \"<scenery_path>\" \"<tileset>\"\n", program );
  printf( "  <scenery_path> is the path to X-Plane's Custom Scenery folder.\n    Example: C:\\X-Plane 12\\Custom Scenery\n" );
  printf( "  <tileset> is the coordinates of the tileset to download and process.\n    Example: +57-006\n" );
}

int
main( int     argc,
      char ** argv ) {

  FXPO_LOG_TITLE( "fxpo: fast x-plane orthoimages" );

  /* Process scenery path and tile coordinates. */
  if( argc < 3 ) {
    FXPO_LOG_ERROR( "missing arguments" );
    print_usage( argv[0] );
    return EXIT_FAILURE;
  }

  const char * scenery_path = argv[1];
  const char * tileset      = argv[2];

  if( fxpo_nvtt3_is_cuda_enabled() ) FXPO_LOG_INFO( "CUDA acceleration enabled" );
  else FXPO_LOG_WARN( "no CUDA acceleration" );

  const size_t max_parallel = omp_get_max_threads();
  FXPO_LOG_INFO( "thread pool size=%zu", max_parallel );

  struct fxpo_tile_t ** tiles;

  FXPO_LOG_INFO( "searching scenery_path=\"%s\" tileset=%s", scenery_path, tileset );
  const size_t tile_num = fxpo_ortho_find_tiles( scenery_path, tileset, &tiles );
  if( tile_num == 0 ) {
    FXPO_LOG_ERROR( "no terrain files found in scenery_path=\"%s\" tileset=%s", scenery_path, tileset );
    return EXIT_FAILURE;
  }

  FXPO_LOG_INFO( "loaded %zu tiles", tile_num );

  /* Initialise libraries and global context. */
  fxpo_http_init();
  fxpo_nvtt3_init();

  struct fxpo_nvtt3_context_t nvtt_ctx;
  fxpo_nvtt3_context_new( &nvtt_ctx );

  /* Use OpenMP to parallelise tile processing. */
  bool    abort = false;
  int32_t it;
  #pragma omp parallel
  {
    struct fxpo_http_multi_context_t http_ctx;
    fxpo_http_multi_context_new( &http_ctx, max_parallel, CHUNK_SIZE * 4 );

    #pragma omp for
    for( it = 0; it < tile_num; it++ ) {
      /* MSVC only supports OpenMP 2.0 that has no proper cancellation support.
         `break` statements in for loops are not allowed by standard OMP however MSVC's implementation
         allows it and exits the current thread's set of iterations which is exactly the goal here. */
      if( abort ) break;

      const struct fxpo_tile_t * const tile = tiles[it];

      FXPO_LOG_INFO( "building chunks for tile x=%u y=%u zoom_level=%u", tile->x, tile->y, tile->zoom_level );

      /* Pixels of the orthophoto for a tile. This is a 4096x4096 image. */
      uint8_t * const tile_imgbuf = fxpo_aligned_malloc( TILE_SIZE * COLOUR_CHANNELS );

      char quadkey[MAX_QUADKEY_LENGTH] = {0};

      char                    urls[CHUNKS_PER_TILE][MAX_URL_LENGTH];
      struct fxpo_http_data_t res[CHUNK_SIZE];
      struct fxpo_chunk_t     chunks[CHUNK_SIZE];

      uint8_t * imgbuf = NULL;
      size_t    imgbuf_len;

      /* Each tile has 256 chunks (16x16). */
      for( uint8_t yo = 0; yo < CHUNKS_PER_TILE_SIDE; yo++ ) {
        for( uint8_t xo = 0; xo < CHUNKS_PER_TILE_SIDE; xo++ ) {
          const size_t i = xo*CHUNKS_PER_TILE_SIDE + yo;

          chunks[i] = (struct fxpo_chunk_t) {
            .x          = tile->x + xo,
            .y          = tile->y + yo,
            .zoom_level = tile->zoom_level,
            .found      = false,
          };

          fxpo_ortho_tile2quadkey( chunks[i].x, chunks[i].y, chunks[i].zoom_level, &quadkey[0] );
          fxpo_ortho_build_url( tile->provider, quadkey, &urls[i][0], MAX_URL_LENGTH );

          fxpo_http_data_new( &res[i] );
        }
      }

      /* Check if chunk at given zoom level found. Resize if not. */
      FXPO_LOG_DEBUG( "fetching metadata for tile x=%u y=%u zl=%u", tile->x, tile->y, tile->zoom_level );

      bool has_chunks = false;

      do {
        has_chunks = true;

        if( fxpo_http_get_multi( &http_ctx, urls, CHUNKS_PER_TILE, res, true ) != FXPOS_OK ) {
          FXPO_LOG_ERROR( "failed to fetch chunk metadata for tile x=%u y=%u zl=%u", tile->x, tile->y, tile->zoom_level );
          abort = true;
          goto cleanup;
        }

        for( size_t i = 0; i < CHUNK_SIZE; i++ ) {
          struct fxpo_chunk_t * const chunk = &chunks[i];
          if( chunk->found ) continue;

          /* Check if chunk at given zoom level exists. Downsample if not. */
          if( strstr((const char *) res[i].buf, "X-VE-Tile-Info: no-tile" )) {
            FXPO_LOG_DEBUG( "missing chunk at x=%u y=%u for zl=%u. downsampling.", chunk->x, chunk->y, chunk->zoom_level );
            has_chunks = false;

            fxpo_ortho_downsample_chunk( chunk );
            fxpo_ortho_tile2quadkey( chunk->x, chunk->y, chunk->zoom_level, &quadkey[0] );
            fxpo_ortho_build_url( tile->provider, quadkey, &urls[i][0], MAX_URL_LENGTH );

            /* printf( "%u %u %u URL: %s\n", chunk->x, chunk->y, chunk->zoom_level, urls[i] ); */

            /* Reset the response buffer to signal fxpo_http_get_multi to make the request again with the new URL. */
            fxpo_http_data_reset( &res[i] );
          } else {
            FXPO_LOG_DEBUG( "found chunk at x=%u y=%u for zl=%u.", chunk->x, chunk->y, chunk->zoom_level );
            chunk->found = true;
          }
        }
      } while( !has_chunks );

      /* Reset HTTP data buffers. */
      for( size_t i = 0; i < CHUNK_SIZE; i++ ) fxpo_http_data_reset( &res[i] );

      FXPO_LOG_DEBUG( "fetching images for tile x=%u y=%u", tile->x, tile->y );
      if( fxpo_http_get_multi( &http_ctx, urls, CHUNKS_PER_TILE, res, false ) != FXPOS_OK ) {
        FXPO_LOG_ERROR( "failed to fetch images for tile x=%u y=%u", tile->x, tile->y );
        abort = true;
        goto cleanup;
      }

      for( uint8_t yo = 0; yo < CHUNKS_PER_TILE_SIDE; yo++ ) {
        for( uint8_t xo = 0; xo < CHUNKS_PER_TILE_SIDE; xo++ ) {
          const size_t i = xo*CHUNKS_PER_TILE_SIDE + yo;

          const struct fxpo_chunk_t * const     chunk = &chunks[i];
          const struct fxpo_http_data_t * const data  = &res[i];

          FXPO_LOG_DEBUG( "processing JPEG image for url=%s size=%zu", urls[i], data->size );

          uint8_t downsample = tile->zoom_level - chunk->zoom_level;
          if( downsample > 0 ) {
            uint32_t x, y, w, h;
            fxpo_ortho_chunk_bbox( chunk->x, chunk->y, tile->x + xo, tile->y + yo, downsample, &x, &y, &w, &h );

            /* Decode a portion of the JPEG image specified by the crop bounding box. */
            if( fxpo_jpeg_cropped_decode( data->buf, data->size, &imgbuf, &imgbuf_len, x, y, w, h ) == FXPOS_OK ) {
              FXPO_LOG_DEBUG( "cropped decoded JPEG image to pixel buffer size=%zu", imgbuf_len );

              /* Upsample cropped image to full chunk size using Catmull-Rom filter. */
              uint8_t * const resized_imgbuf = fxpo_aligned_malloc( CHUNK_SIZE * CHUNK_SIZE * COLOUR_CHANNELS );
              stbir_resize_uint8_linear( imgbuf, (int)w, (int)h, 0, resized_imgbuf, CHUNK_SIZE, CHUNK_SIZE, 0, STBIR_RGBA );
              aligned_free( imgbuf );
              imgbuf = resized_imgbuf;
            } else {
              FXPO_LOG_ERROR( "failed to cropped decode JPEG image for url=%s", urls[i] );
              abort = true;
              goto cleanup;
            }
          } else {
            if( fxpo_jpeg_decode( data->buf, data->size, &imgbuf, &imgbuf_len ) == FXPOS_OK ) {
              FXPO_LOG_DEBUG( "decoded JPEG image to pixel buffer size=%zu", imgbuf_len );
            } else {
              FXPO_LOG_ERROR( "failed to decode JPEG image for url=%s", urls[i] );
              abort = true;
              goto cleanup;
            }
          }

          for( size_t j = 0; j < CHUNK_SIZE; j++ ) {
            memcpy( &tile_imgbuf[yo*TILE_WIDTH*CHUNK_SIZE*COLOUR_CHANNELS + j*TILE_HEIGHT*COLOUR_CHANNELS + xo*CHUNK_SIZE*COLOUR_CHANNELS],
                    &imgbuf[j*CHUNK_SIZE*COLOUR_CHANNELS],
                    CHUNK_SIZE*COLOUR_CHANNELS );
          }

          aligned_free( imgbuf );
          imgbuf = NULL;
        }
      }

      FXPO_LOG_DEBUG( "built tile x=%u y=%u zl=%u", tile->x, tile->y, tile->zoom_level );

      char dds_path[MAX_PATH_LENGTH];
      fxpo_ortho_build_dds_path( scenery_path, tileset, tile, dds_path, sizeof(dds_path) );

      FXPO_LOG_DEBUG( "compressing tile to dds=%s", dds_path );
      fxpo_nvtt3_compress( &nvtt_ctx, TILE_WIDTH, TILE_HEIGHT, tile_imgbuf, dds_path );
      FXPO_LOG_INFO( "saved compressed tile to dds=%s", dds_path );

cleanup:
      for( size_t i = 0; i < CHUNKS_PER_TILE; i++ ) fxpo_http_data_free( &res[i] );
      if( imgbuf != NULL ) aligned_free( imgbuf );
      aligned_free( tile_imgbuf );
    } /* for end */

    fxpo_http_multi_context_free( &http_ctx );
  } /* omp parallel end */

  /* Clean up. */
  for( size_t i = 0; i < tile_num; i++ ) free( tiles[i] );
  free( tiles );
  fxpo_http_clean();
  fxpo_nvtt3_context_free( &nvtt_ctx );

  if( abort ) {
    FXPO_LOG_ERROR( "aborted!" );
    return EXIT_FAILURE;
  }

  FXPO_LOG_INFO( "done!" );
  return EXIT_SUCCESS;
}
