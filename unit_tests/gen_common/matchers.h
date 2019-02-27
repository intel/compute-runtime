/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "gmock/gmock.h"

#include <cstdint>
#include <string.h>

MATCHER_P2(MemCompare, memory, size, "") {
    return memcmp(arg, memory, size) == 0;
}

MATCHER_P(MemoryZeroed, size, "") {
    size_t sizeLeft = (size_t)size;
    bool memoryZeroed = true;
    while (--sizeLeft) {
        uint8_t *pMem = (uint8_t *)arg;
        if (pMem[sizeLeft] != 0) {
            memoryZeroed = false;
            break;
        }
    }
    return memoryZeroed;
}
