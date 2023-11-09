#ifndef FXPO_COMMON_H
#define FXPO_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
/* Math constants are not defined in standard C. */
#define _USE_MATH_DEFINES
#include <math.h>
#include <time.h>
#include <omp.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define strdup _strdup
#endif

#define MAX_PATH_LENGTH 1023

enum fxpo_status {
  FXPOS_OK = 0,
  FXPOS_NULL_POINTER,
  FXPOS_INVALID_STATE,
};

#endif