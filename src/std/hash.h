#pragma once

#include "common.h"

u32 hashFnv132(const void *data, usize len);
u64 hashFnv164(const void *data, usize len);