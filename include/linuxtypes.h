#ifdef __linux__
#include <linux/types.h>
#else
#ifndef LINUX_TYPES_ADDED
#define LINUX_TYPES_ADDED
#include <sys/types.h>
typedef uint64_t __u64;
typedef uint32_t __u32;
typedef uint16_t __u16;
typedef uint8_t __u8;
typedef int64_t __s64;
#endif
#endif
