#include "fxpo_common.h"
#include "fxpo_log.h"

static const char * const FXPO_LOG_LEVEL_STR[] = {
  "INFO",
  "INFO",
  "WARN",
  "ERROR",
  "DEBUG",
};

static inline const char *
fxpo_log_level_str( const enum fxpo_log_level level ) {

  if( level >= FXPO_LOG_LEVEL_TITLE && level <= FXPO_LOG_LEVEL_DEBUG ) return FXPO_LOG_LEVEL_STR[level];
  return "UNKNOWN";
}

void
fxpo_log( FILE * const              stream,
          const enum fxpo_log_level level,
          const char *              fmt,
          ... ) {

  char buf[20];

  const time_t            now     = time( NULL );
  const struct tm * const tm_info = localtime( &now );

  strftime( buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm_info );

  va_list args;
  va_start( args, fmt );
  #pragma omp critical
  {
#ifdef _WIN32
    HANDLE console    = NULL;
    DWORD  std_handle = STD_OUTPUT_HANDLE;
    WORD   colour     = 0;

    switch( level ) {
      case FXPO_LOG_LEVEL_TITLE: colour = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;
      case FXPO_LOG_LEVEL_WARN:  colour = FOREGROUND_RED | FOREGROUND_GREEN; break;
      case FXPO_LOG_LEVEL_ERROR: colour = FOREGROUND_RED; std_handle = STD_ERROR_HANDLE; break;
      case FXPO_LOG_LEVEL_DEBUG: colour = FOREGROUND_INTENSITY; break;
      default: break;
    }

    if( colour > 0 ) {
      console = GetStdHandle( std_handle );
      SetConsoleTextAttribute( console, colour );
    }
#endif
    fprintf( stream, "%s %s [%d/%d] ", buf, fxpo_log_level_str( level ), omp_get_num_threads(), omp_get_thread_num()+1 );
    vfprintf( stream, fmt, args );
#ifdef _WIN32
    if( console != NULL ) SetConsoleTextAttribute( console, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE );
#endif
  }
  va_end( args );
}
