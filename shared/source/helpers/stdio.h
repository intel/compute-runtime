/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#ifndef _WIN32
#ifndef __STDC_LIB_EXT1__
#if __STDC_WANT_LIB_EXT1__ != 1

#include <cstdio>
#include <errno.h>

inline int fopen_s(FILE **pFile, const char *filename, const char *mode) {
    if ((pFile == nullptr) || (filename == nullptr) || (mode == nullptr)) {
        return -EINVAL;
    }

    *pFile = fopen(filename, mode);
    if (*pFile == nullptr) {
        return -errno;
    }

    return 0;
}

#endif
#endif
#endif
