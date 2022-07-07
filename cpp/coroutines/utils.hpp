#pragma once

#ifdef __linux__
#define ALWAYS_INLINE __attribute__((always_inline))
#else
#define ALWAYS_INLINE __forceinline
#endif