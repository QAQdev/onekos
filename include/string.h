#pragma once

#include "stddef.h"
#include "types.h"

void *memset(void *, int, uint64);

void *memcpy(void *dst, const void *src, size_t len);