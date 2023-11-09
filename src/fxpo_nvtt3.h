#ifndef FXPO_NVTT3_H
#define FXPO_NVTT3_H

#include <nvtt/nvtt_wrapper.h>
#include "fxpo_common.h"

struct fxpo_nvtt3_context_t {
  NvttContext *            context;
  NvttCompressionOptions * comp_opts;
};

void
fxpo_nvtt3_init();

void
fxpo_nvtt3_context_new( struct fxpo_nvtt3_context_t * ctx );

void
fxpo_nvtt3_context_free( struct fxpo_nvtt3_context_t * ctx );

enum fxpo_status
fxpo_nvtt3_compress( const struct fxpo_nvtt3_context_t * ctx,
                     uint32_t                            width,
                     uint32_t                            height,
                     const uint8_t *                     data,
                     const char *                        outfile );

bool
fxpo_nvtt3_is_cuda_enabled();

#endif