/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#if _WIN32
#include <windows.h>

#include <winternl.h>

#pragma warning(push)
#pragma warning(disable : 4005)
#include <ntstatus.h>
#pragma warning(pop)
// There is a conflict with max/min defined as macro in windows headers with std::max/std::min
#undef min
#undef max
#undef RegOpenKeyExA
#undef RegQueryValueExA
#pragma warning(disable : 4273)
#else
#include <cstdint>
#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-value"
#endif

#include <winadapter.h>

#if __clang__
#pragma clang diagnostic pop
#endif
#include <x86intrin.h>

#define STATUS_GRAPHICS_NO_VIDEO_MEMORY ((NTSTATUS)0xC01E0100L)

#define PAGE_NOACCESS 0x01
#define PAGE_READWRITE 0x04
#define MEM_COMMIT 0x00001000
#define MEM_RESERVE 0x00002000
#define MEM_TOP_DOWN 0x00100000
#define MEM_RELEASE 0x00008000
#define MEM_FREE 0x00010000

#define DXGI_RESOURCE_PRIORITY_NORMAL 0x78000000
#define DXGI_RESOURCE_PRIORITY_HIGH 0xa0000000
#define DXGI_RESOURCE_PRIORITY_MAXIMUM 0xc8000000
#endif

#define NULL_HANDLE 0U
