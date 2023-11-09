#include "fxpo_http.h"
#include "fxpo_log.h"
#include "fxpo_ortho.h"
#include "fxpo_alloc.h"

#define HTTP_TIMEOUT_MS     1000
#define INITIAL_BUFFER_SIZE 15000 /* Average chunk JPEG size is 11kb */
#define USER_AGENT_EDGE_118 "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/118.0.0.0 Safari/537.36 Edg/118.0.2088.76"

void
fxpo_http_init() {

  curl_global_init( CURL_GLOBAL_ALL );
}

void
fxpo_http_clean() {

  curl_global_cleanup();
}

void
fxpo_http_multi_context_new( struct fxpo_http_multi_context_t * const ctx,
                             const size_t                             num_handles,
                             const size_t                             max_open_conns ) {

  ctx->multi_handle = curl_multi_init();
  curl_multi_setopt( ctx->multi_handle, CURLMOPT_MAXCONNECTS, max_open_conns );

  ctx->easy_handles_len = num_handles;
  ctx->easy_handles     = fxpo_malloc( sizeof(CURL *) * num_handles );
  for( size_t i = 0; i < num_handles; i++ ) ctx->easy_handles[i] = curl_easy_init();
}

void
fxpo_http_multi_context_free( struct fxpo_http_multi_context_t * const ctx ) {

  for( size_t i = 0; i < ctx->easy_handles_len; i++ ) curl_easy_cleanup( ctx->easy_handles[i] );
  free( ctx->easy_handles );
  ctx->easy_handles_len = 0;

  curl_multi_cleanup( ctx->multi_handle );
}

void
fxpo_http_data_new( struct fxpo_http_data_t * const data ) {

  data->buf     = fxpo_malloc( INITIAL_BUFFER_SIZE );
  data->buf_len = INITIAL_BUFFER_SIZE;
  data->size    = 0;
}

void
fxpo_http_data_free( struct fxpo_http_data_t * const data ) {

  free( data->buf );
  data->buf_len = 0;
  data->size    = 0;
}

void
fxpo_http_data_reset( struct fxpo_http_data_t * const data ) {

  /* No need to set the buffer contents to 0, we'll overwrite it anyway. */
  data->size = 0;
}

static size_t
fxpo_write_callback( char * chunk,
                     size_t size,
                     size_t nmemb,
                     void * userp ) {

  struct fxpo_http_data_t * const data  = (struct fxpo_http_data_t *)userp;
  size_t                          rsize = size * nmemb;

  size_t req_len = data->size + rsize + 1;

  if( data->buf_len < req_len ) {
    data->buf     = fxpo_realloc( data->buf, req_len );
    data->buf_len = req_len;
  }

  memcpy( &(data->buf[data->size]), chunk, rsize );
  data->size           += rsize;
  data->buf[data->size] = 0;

  return rsize;
}

static void
fxpo_http_add_request( CURLM * const                   multi_handle,
                       CURL * const                    curl,
                       const char *                    url,
                       struct fxpo_http_data_t * const res,
                       const bool                      is_head ) {

  curl_easy_setopt( curl, CURLOPT_URL, url );
  if( is_head ) curl_easy_setopt( curl, CURLOPT_HEADERDATA, res );
  else curl_easy_setopt( curl, CURLOPT_WRITEDATA, res );

  curl_multi_add_handle( multi_handle, curl );
}

enum fxpo_status
fxpo_http_get_multi( const struct fxpo_http_multi_context_t * const ctx,
                     const char                                     urls[][MAX_URL_LENGTH],
                     const size_t                                   url_len,
                     struct fxpo_http_data_t * const                res,
                     const bool                                     is_head ) {

  CURLM * const multi_handle = ctx->multi_handle;
  CURL ** const curls        = ctx->easy_handles;
  size_t i, j;
  size_t completed = 0;

  if( curls == NULL || multi_handle == NULL ) {
    FXPO_LOG_ERROR( "fxpo_http_get_multi(): multi ctx is not initialized" );
    return FXPOS_NULL_POINTER;
  }

  /* Add initial requests.
     `i` tracks position in `urls` and `res`, `j` tracks position in the CURL handle array. */
  for( i = 0, j = 0; j < ctx->easy_handles_len && i < url_len; i++ ) {
    /* Mark request as completed if there's already response data. */
    if( res[i].size > 0 ) {
      completed++;
      continue;
    }

    /* Re-use handles. */
    curl_easy_reset( curls[j] );

    if( is_head ) curl_easy_setopt( curls[j], CURLOPT_NOBODY, 1L );
    curl_easy_setopt( curls[j], CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1 );
    curl_easy_setopt( curls[j], CURLOPT_USERAGENT, USER_AGENT_EDGE_118 );
    curl_easy_setopt( curls[j], CURLOPT_ACCEPT_ENCODING, "" );   /* Enable all supported encodings. */
    curl_easy_setopt( curls[j], CURLOPT_WRITEFUNCTION, fxpo_write_callback );

    /* Uncomment to enable libcurl verbose logging. */
    /* curl_easy_setopt( curls[i], CURLOPT_VERBOSE, 1L ); */

    fxpo_http_add_request( multi_handle, curls[j++], urls[i], &res[i], is_head );
  }

  int32_t running_handles = 0;
  int32_t msgs_in_queue   = 0;
  const CURLMsg * msg;
  CURLMcode code;

  do {
    code = curl_multi_perform( multi_handle, &running_handles );
    if( code == CURLM_OK && running_handles > 0 ) {
      code = curl_multi_poll( multi_handle, NULL, 0, HTTP_TIMEOUT_MS, NULL );
    }

    if( code != CURLM_OK ) {
      FXPO_LOG_ERROR( "fxpo_http_get_multi(): curl multi operation failed (%d): %s", code, curl_multi_strerror( code ) );
      return FXPOS_INVALID_STATE;
    }

    do {
      msg = curl_multi_info_read( multi_handle, &msgs_in_queue );
      if( msg && msg->msg == CURLMSG_DONE ) {
        curl_multi_remove_handle( multi_handle, msg->easy_handle );

        if( msg->data.result == CURLE_OPERATION_TIMEDOUT ) {
          FXPO_LOG_WARN( "fxpo_http_get_multi(): curl operation timed out, retrying" );

          /* Retry indefinitely. TODO a more robust retry logic. */
          curl_multi_add_handle( multi_handle, msg->easy_handle );
        } else if( msg->data.result != CURLE_OK ) {
          FXPO_LOG_ERROR( "fxpo_http_get_multi(): curl operation failed (%d): %s", msg->data.result, curl_easy_strerror( msg->data.result ) );
          return FXPOS_INVALID_STATE;
        } else {
          completed++;

          /* Find the next request that needs to be made. */
          for( ; res[i].size > 0 && i < url_len; i++, completed++ );
          /* If there are more requests left, re-use the just completed handle for it. */
          if( i < url_len ) {
            fxpo_http_add_request( multi_handle, msg->easy_handle, urls[i], &res[i], is_head );
            i++;
          }
        }
      }
    } while( msg );
  } while( completed < url_len );

  return FXPOS_OK;
}

enum fxpo_status
fxpo_http_get( const char *                    url,
               struct fxpo_http_data_t * const res ) {


  CURL * const curl = curl_easy_init();
  if( curl == NULL ) return FXPOS_NULL_POINTER;

  curl_easy_setopt( curl, CURLOPT_URL, url );
  curl_easy_setopt( curl, CURLOPT_USERAGENT, USER_AGENT_EDGE_118 );
  curl_easy_setopt( curl, CURLOPT_ACCEPT_ENCODING, "" );
  curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, fxpo_write_callback );
  curl_easy_setopt( curl, CURLOPT_WRITEDATA, res );

  const CURLcode ret = curl_easy_perform( curl );

  curl_easy_cleanup( curl );

  if( ret != CURLE_OK ) {
    FXPO_LOG_ERROR( "fxpo_http_get(): curl_easy_perform() failed: %s", curl_easy_strerror( ret ) );
    return FXPOS_INVALID_STATE;
  }

  return FXPOS_OK;
}
