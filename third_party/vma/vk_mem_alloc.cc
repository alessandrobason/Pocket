#include "std/logging.h"

#define VMA_DEBUG_LOG(...) info("(VMA) " __VA_ARGS__)

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"