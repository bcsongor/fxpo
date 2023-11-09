#ifndef FXPO_JPEG_H
#define FXPO_JPEG_H

#include <turbojpeg.h>
#include "fxpo_common.h"

#define PIXEL_FORMAT    TJPF_BGRA
#define COLOUR_CHANNELS tjPixelSize[TJPF_BGRA]

enum fxpo_status
fxpo_jpeg_decode( uint8_t *  jpegbuf,
                  size_t     jpegbuf_len,
                  uint8_t ** imgbuf,
                  size_t *   imgbuf_len );

enum fxpo_status
fxpo_jpeg_cropped_decode( uint8_t *  jpegbuf,
                          size_t     jpegbuf_len,
                          uint8_t ** imgbuf,
                          size_t *   imgbuf_len,
                          uint32_t   crop_x,
                          uint32_t   crop_y,
                          uint32_t   crop_w,
                          uint32_t   crop_h );

#endif