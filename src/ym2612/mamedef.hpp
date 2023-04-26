#pragma once

#include <cstdint>

/* offsets and addresses are 32-bit (for now...) */
using offs_t = uint32_t;

/* stream_sample_t is used to represent a single sample in a sound stream */
using stream_sample_t = int32_t;

#if defined(_MSC_VER)
//#define INLINE	static __forceinline
#define INLINE	static __inline
#elif defined(__GNUC__)
#define INLINE    static __inline__
#else
#define INLINE	static inline
#endif
#define M_PI    3.14159265358979323846

extern stream_sample_t *DUMMYBUF[];
