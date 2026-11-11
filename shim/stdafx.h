#pragma once
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;
#define VERIFY(x) ((void)0)
#ifndef __declspec
#endif
#include <stdio.h>
#define IC inline
#define XRCORE_API
#define xr_malloc malloc
#define xr_realloc realloc
#define xr_free free
