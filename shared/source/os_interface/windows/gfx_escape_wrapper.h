/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/windows/windows_wrapper.h"

#ifdef _WIN32
#include "gfxEscape.h"
#else // !_WIN32
typedef struct GFX_ESCAPE_HEADER {
    union {
        struct
        {
            unsigned int Size;
            unsigned int CheckSum;
            unsigned int EscapeCode;

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
