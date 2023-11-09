#include "fxpo_log.h"
#include "fxpo_nvtt3.h"

bool
fxpo_nvtt3_is_cuda_enabled() {

  return nvttIsCudaSupported() == NVTT_True;
}

void
fxpo_nvtt3_init() {

  nvttUseCurrentDevice();
}

void
fxpo_nvtt3_context_new( struct fxpo_nvtt3_context_t * const ctx ) {

  ctx->context = nvttCreateContext();
  if( nvttIsCudaSupported() == NVTT_True ) {
    nvttSetContextCudaAcceleration( ctx->context, NVTT_True );
  }

  ctx->comp_opts = nvttCreateCompressionOptions();
  nvttSetCompressionOptionsFormat( ctx->comp_opts, NVTT_Format_BC1 );
}

void
fxpo_nvtt3_context_free( struct fxpo_nvtt3_context_t * const ctx ) {

  nvttDestroyContext( ctx->context );
  nvttDestroyCompressionOptions( ctx->comp_opts );
}

enum fxpo_status
fxpo_nvtt3_compress( const struct fxpo_nvtt3_context_t * const ctx,
                     const uint32_t                            width,
                     const uint32_t                            height,
                     const uint8_t *                           data,
                     const char *                              outfile ) {

  enum fxpo_status state = FXPOS_OK;

  NvttSurface * const surface = nvttCreateSurface();
  NvttOutputOptions * out_opt = NULL;

  if( nvttSurfaceSetImageData( surface, NVTT_InputFormat_BGRA_8UB, (int)width, (int)height, 1, data, NVTT_False, NULL ) == NVTT_False ) {
    FXPO_LOG_ERROR( "fxpo_nvtt3_compress(): failed set image surface" );
    state = FXPOS_INVALID_STATE;
    goto cleanup;
  }

  out_opt = nvttCreateOutputOptions();
  nvttSetOutputOptionsFileName( out_opt, outfile );

  const int mips = nvttSurfaceCountMipmaps( surface, 1 );

  /* Write DDS headers. */
  if( nvttContextOutputHeader( ctx->context, surface, mips, ctx->comp_opts, out_opt ) == NVTT_False ) {
    FXPO_LOG_ERROR( "fxpo_nvtt3_compress(): failed to output DDS header surface" );
    state = FXPOS_INVALID_STATE;
    goto cleanup;
  }

  for( int mip = 0; mip < mips; mip++ ) {
    if( nvttContextCompress( ctx->context, surface, 0, mip, ctx->comp_opts, out_opt ) == NVTT_False ) {
      FXPO_LOG_ERROR( "fxpo_nvtt3_compress(): failed to compress mip %d", mip );
      state = FXPOS_INVALID_STATE;
      goto cleanup;
    }

    if( mip == mips - 1 ) break;

    nvttSurfaceToLinearFromSrgb( surface, NULL );
    if( nvttSurfaceBuildNextMipmapDefaults( surface, NVTT_MipmapFilter_Box, 1, NULL ) == NVTT_False ) {
      FXPO_LOG_ERROR( "fxpo_nvtt3_compress(): failed to build next mip %d", mip+1 );
      state = FXPOS_INVALID_STATE;
      goto cleanup;
    }
    nvttSurfaceToSrgb( surface, NULL );
  }

cleanup:
  if( out_opt != NULL ) nvttDestroyOutputOptions( out_opt );
  if( surface != NULL ) nvttDestroySurface( surface );

  return state;
}
