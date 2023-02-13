#pragma once

#include <stdint.h>

// typedefs to use MAME's (U)INTxx types (copied from MAME\src\ods\odscomm.h)
// 8-bit values
typedef uint8_t 						UINT8;
typedef int8_t 						INT8;

// 16-bit values
typedef uint16_t 						UINT16;
typedef int16_t 						INT16;

// 32-bit values
#ifndef _WINDOWS_H
typedef uint32_t 						UINT32;
typedef int32_t 							INT32;

// 64-bit values
typedef uint64_t 	UINT64;
typedef int64_t 		INT64;
#endif

/* offsets and addresses are 32-bit (for now...) */
typedef uint32_t  offs_t;

/* stream_sample_t is used to represent a single sample in a sound stream */
typedef int32_t stream_sample_t;

#if defined(_MSC_VER)
//#define INLINE	static __forceinline
#define INLINE	static __inline
#elif defined(__GNUC__)
#define INLINE	static __inline__
#else
#define INLINE	static inline
#endif
#define M_PI	3.14159265358979323846

typedef uint8_t	(*read8_device_func)  (offs_t offset);
typedef void	(*write8_device_func) (offs_t offset, uint8_t data);

#ifdef _DEBUG
#define logerror	printf
#else
#define logerror
#endif

extern stream_sample_t* DUMMYBUF[];
