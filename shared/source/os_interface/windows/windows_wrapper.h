/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#if _WIN32
#include <windows.h>
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
LSTATUS APIENTRY RegOpenKeyExA(
    HKEY hKey,
    LPCSTR lpSubKey,
    DWORD ulOptions,
    REGSAM samDesired,
    PHKEY phkResult);
LSTATUS APIENTRY RegQueryValueExA(
    HKEY hKey,
    LPCSTR lpValueName,
    LPDWORD lpReserved,
    LPDWORD lpType,
    LPBYTE lpData,
    LPDWORD lpcbData);
#else
#include <cstdint>
#include <x86intrin.h>
#define C_ASSERT(e) static_assert(e, #e)
#endif
