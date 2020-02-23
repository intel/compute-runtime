/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#define PATH_SEPARATOR '\\'

// For now we need to keep this file clean of OS specific #includes.
// Only issues to address portability should be covered here.

namespace Os {
// OS GDI name
extern const char *gdiDllName;
}; // namespace Os
