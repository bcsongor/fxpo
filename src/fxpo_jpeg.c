#include "fxpo_jpeg.h"
#include "fxpo_log.h"
#include "fxpo_alloc.h"

enum fxpo_status
fxpo_jpeg_decode( uint8_t * const jpegbuf,
                  const size_t    jpegbuf_len,
                  uint8_t **      imgbuf,
                  size_t *        imgbuf_len ) {

  tjhandle h = tj3Init( TJINIT_DECOMPRESS );

  if( h == NULL ) {
    FXPO_LOG_ERROR( "fxpo_jpeg_decode(): failed to init decompression handle" );
    return FXPOS_NULL_POINTER;
  }

  if( tj3DecompressHeader( h, jpegbuf, jpegbuf_len ) < 0 ) {
    FXPO_LOG_ERROR( "fxpo_jpeg_decode(): failed to decompress JPEG header: %s", tj3GetErrorStr( h ) );
    tj3Destroy( h );
    return FXPOS_INVALID_STATE;
  }

  const int width      = tj3Get( h, TJPARAM_JPEGWIDTH );
  const size_t dst_len = width * tj3Get( h, TJPARAM_JPEGHEIGHT ) * COLOUR_CHANNELS;
  uint8_t * const dst  = fxpo_aligned_malloc(dst_len);

  if( tj3Decompress8( h, jpegbuf, jpegbuf_len, dst, width*COLOUR_CHANNELS, PIXEL_FORMAT ) < 0 ) {
    FXPO_LOG_ERROR( "fxpo_jpeg_decode(): failed to decompress JPEG image: %s", tj3GetErrorCode( h ) );
    tj3Destroy( h );
    free( dst );
    return FXPOS_INVALID_STATE;
  }

  *imgbuf     = dst;
  *imgbuf_len = dst_len;

  tj3Destroy( h );

  return FXPOS_OK;
}

enum fxpo_status
fxpo_jpeg_cropped_decode( uint8_t * const jpegbuf,
                          const size_t    jpegbuf_len,
                          uint8_t **      imgbuf,
                          size_t *        imgbuf_len,
                          const uint32_t  crop_x,
                          const uint32_t  crop_y,
                          const uint32_t  crop_w,
                          const uint32_t  crop_h ) {

  tjhandle h = tj3Init( TJINIT_DECOMPRESS );

  if( h == NULL ) {
    FXPO_LOG_ERROR( "fxpo_jpeg_decode(): failed to init decompression handle" );
    return FXPOS_NULL_POINTER;
  }

  if( tj3DecompressHeader( h, jpegbuf, jpegbuf_len ) < 0 ) {
    FXPO_LOG_ERROR( "fxpo_jpeg_decode(): failed to decompress JPEG header: %s", tj3GetErrorStr( h ) );
    tj3Destroy( h );
    return FXPOS_INVALID_STATE;
  }

  /* Only decompress the specified region which be divisible by MCU block size (8x8). */
  const tjregion region = { .x = (int)crop_x, .y = (int)crop_y, .w = (int)crop_w, .h = (int)crop_h };
  if( tj3SetCroppingRegion( h, region ) < 0 ) {
    FXPO_LOG_ERROR( "fxpo_jpeg_decode(): failed to set cropping region: %s", tj3GetErrorCode( h ) );
    tj3Destroy( h );
    return FXPOS_INVALID_STATE;
  }

  const size_t dst_len = crop_w * crop_h * COLOUR_CHANNELS;
  uint8_t * const dst  = fxpo_aligned_malloc(dst_len);

  if( tj3Decompress8( h, jpegbuf, jpegbuf_len, dst,  0, PIXEL_FORMAT ) < 0 ) {
    FXPO_LOG_ERROR( "fxpo_jpeg_decode(): failed to decompress JPEG image: %s", tj3GetErrorCode( h ) );
    tj3Destroy( h );
    free( dst );
    return FXPOS_INVALID_STATE;
  }

  *imgbuf     = dst;
  *imgbuf_len = dst_len;

  tj3Destroy( h );

  return FXPOS_OK;
}
