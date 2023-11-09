#ifndef FXPO_HTTP_H
#define FXPO_HTTP_H

#include <curl/curl.h>
#include "fxpo_common.h"

#define MAX_URL_LENGTH 255

struct fxpo_http_multi_context_t {
  CURLM * multi_handle;
  CURL ** easy_handles;
  size_t  easy_handles_len;
};

struct fxpo_http_data_t {
  /* Response data. */
  uint8_t * buf;
  /* Allocated length of buf. */
  size_t buf_len;
  /* Size of useful data in buf. */
  size_t size;
};

/* fxpo_http_init initialises the context required to make HTTP requests. */
void
fxpo_http_init();

/* fxpo_http_clean cleans up the HTTP request context. */
void
fxpo_http_clean();

void
fxpo_http_multi_context_new( struct fxpo_http_multi_context_t * ctx,
                             size_t                             num_handles,
                             size_t                             max_open_conns );

void
fxpo_http_multi_context_free( struct fxpo_http_multi_context_t * ctx );

/* fxpo_http_data_new allocates the data structure required to store the result of HTTP requests.
   This can be reused across multiple HTTP requests. */
void
fxpo_http_data_new( struct fxpo_http_data_t * data );

/* fxpo_http_data_free releases memory associated with the result of an HTTP request. */
void
fxpo_http_data_free( struct fxpo_http_data_t * data );

void
fxpo_http_data_reset( struct fxpo_http_data_t * data );

/* fxpo_http_get sends a synchronous HTTP GET request to the given url and returns the response
   buffer and length in res. res must be initialised via fxpo_http_data_new. */
enum fxpo_status
fxpo_http_get( const char *              url,
               struct fxpo_http_data_t * res );

/* fxpo_http_get_multi sends a multiple HTTP GET request to the given urls and returns the response
   buffers in res. Each res must be initialised via fxpo_http_data_new. */
enum fxpo_status
fxpo_http_get_multi( const struct fxpo_http_multi_context_t * ctx,
                     const char                               urls[][MAX_URL_LENGTH],
                     size_t                                   url_len,
                     struct fxpo_http_data_t *                res,
                     bool                                     is_head );

#endif