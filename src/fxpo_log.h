#ifndef FXPO_LOG_H
#define FXPO_LOG_H

#include <stdio.h>

enum fxpo_log_level {
  FXPO_LOG_LEVEL_TITLE,
  FXPO_LOG_LEVEL_INFO,
  FXPO_LOG_LEVEL_WARN,
  FXPO_LOG_LEVEL_ERROR,
  FXPO_LOG_LEVEL_DEBUG,
};

void
fxpo_log( FILE *              stream,
          enum fxpo_log_level level,
          const char *        fmt,
          ... );

#define FXPO_LOG_TITLE( fmt, ... ) fxpo_log( stdout, FXPO_LOG_LEVEL_TITLE, fmt "\n", ## __VA_ARGS__ )
#define FXPO_LOG_INFO( fmt, ... )  fxpo_log( stdout, FXPO_LOG_LEVEL_INFO,  fmt "\n", ## __VA_ARGS__ )
#define FXPO_LOG_WARN( fmt, ... )  fxpo_log( stdout, FXPO_LOG_LEVEL_WARN,  fmt "\n", ## __VA_ARGS__ )
#define FXPO_LOG_ERROR( fmt, ... ) fxpo_log( stderr, FXPO_LOG_LEVEL_ERROR, fmt "\n", ## __VA_ARGS__ )

#ifdef NDEBUG
#define FXPO_LOG_DEBUG( fmt, ... ) do {} while( 0 )
#else
#define FXPO_LOG_DEBUG( fmt, ... ) fxpo_log( stdout, FXPO_LOG_LEVEL_DEBUG, fmt "\n", ## __VA_ARGS__ )
#endif

#endif