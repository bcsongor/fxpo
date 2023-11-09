#ifndef FXPO_ALLOC_H
#define FXPO_ALLOC_H

#include <stdlib.h>
#include <stdio.h>

#define DEFAULT_ALIGNMENT 16

#ifdef _WIN32
#include <malloc.h>
#define aligned_alloc( align, size ) _aligned_malloc( size, align )
#define aligned_free( ptr )          _aligned_free( ptr )
#endif

static inline void *
fxpo_malloc( const size_t size ) {

  void * ptr = malloc( size );
  if( ptr == NULL && size > 0 ) {
    fprintf( stderr, "malloc out of memory (%zu bytes)\n", size );
    exit( EXIT_FAILURE );
  }
  return ptr;
}

static inline void *
fxpo_aligned_malloc( const size_t size ) {

  void * ptr = aligned_alloc( DEFAULT_ALIGNMENT, size );
  if( ptr == NULL && size > 0 ) {
    fprintf( stderr, "aligned malloc out of memory (%zu bytes)\n", size );
    exit( EXIT_FAILURE );
  }
  return ptr;
}

static inline void *
fxpo_realloc( void * const ptr,
              const size_t size ) {

  void * newptr = realloc( ptr, size );
  if( newptr == NULL && size > 0 ) {
    fprintf( stderr, "realloc out of memory (%zu bytes)\n", size );
    exit( EXIT_FAILURE );
  }

  return newptr;
}

#endif