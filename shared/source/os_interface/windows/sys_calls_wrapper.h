/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/windows/d3dkmthk_wrapper.h"

namespace NEO {
namespace SysCalls {
BOOL closeHandle(HANDLE hObject);
HANDLE openFileMapping(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName);
LPVOID mapViewOfFile(HANDLE hFileMappingObject, DWORD dwDesiredAccess, DWORD dwFileOffsetHigh, DWORD dwFileOffsetLow, SIZE_T dwNumberOfBytesToMap);
BOOL unmapViewOfFile(LPCVOID lpBaseAddress);
} // namespace SysCalls
} // namespace NEO
