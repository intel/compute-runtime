/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#if _WIN32
#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wignored-pragma-intrinsic"
#pragma clang diagnostic ignored "-Wpragma-pack"
#pragma clang diagnostic ignored "-Wignored-attributes"
#pragma clang diagnostic ignored "-Wmacro-redefined"
#define UNICODE
#endif

#include <Windows.h>

#include <ShlObj.h>
#include <cfgmgr32.h>
#include <winternl.h>
#pragma comment(lib, "cfgmgr32.lib")

#if __clang__
#pragma clang diagnostic pop
#endif

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
#if defined(__x86_64__)
#include <x86intrin.h>
#endif

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
