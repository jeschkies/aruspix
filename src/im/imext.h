

#ifndef __AX_CORE_IM_EXT_H__
#define __AX_CORE_IM_EXT_H__


#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// Byte / short pixel aliases inherited from IM. Kept as typedefs so
// call sites that still spell out `imbyte` / `imushort` continue to
// compile after the IM headers were dropped.
typedef uint8_t  imbyte;
typedef uint16_t imushort;

#if	defined(__cplusplus)
extern "C" {
#endif

int median( int a[], int size, bool sort_array = true );

float medianf( float a[], int size, bool sort_array = true );

int max_val( int a[], int size , int *pos = NULL);

int sum( int a[], int size );

int count( int a[], int size );

void corr( int a[], int b[], int size, int win, int *dec, int *max);

#if defined(__cplusplus)
}
#endif


#endif // __AX_CORE_IM_EXT_H__
