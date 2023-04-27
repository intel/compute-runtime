/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/windows/windows_wrapper.h"

#ifdef _WIN32
#include "gfxEscape.h"
#else // !_WIN32
typedef struct GFX_ESCAPE_HEADER { // NOLINT(readability-identifier-naming)
    union {
        struct
        {
            unsigned int Size;       // NOLINT(readability-identifier-naming)
            unsigned int CheckSum;   // NOLINT(readability-identifier-naming)
            unsigned int EscapeCode; // NOLINT(readability-identifier-naming)

            unsigned int ulReserved;
        };
        struct
        {
            unsigned int ulReserved1;
            unsigned short usEscapeVersion;
            unsigned short usFileVersion;
            unsigned int ulMajorEscapeCode;

            unsigned int uiMinorEscapeCode;
        };
    };
} GFX_ESCAPE_HEADER_T;
#endif
