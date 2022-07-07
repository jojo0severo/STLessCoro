#pragma once

#if defined(__linux__) || defined(__MINGW32__)
#define ALWAYS_INLINE __attribute__((always_inline))
#else
#define ALWAYS_INLINE __forceinline
#endif