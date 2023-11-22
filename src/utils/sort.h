#pragma once

#include "std/common.h"
#include "std/slice.h"
#include "std/arena.h"

void radixSort(u32 *buf, u32 len);
void radixSort(u32 *buf, u32 len, Arena scratch);

void radixSort(void *data, u32 len, u32 stride);
void radixSort(void *data, u32 len, u32 stride, Arena scratch);