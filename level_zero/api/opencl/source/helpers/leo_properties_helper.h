/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "CL/cl.h"

#include <array>

namespace NEO {

namespace LEO {

using MemObjSizeArray = std::array<size_t, 3>;
using MemObjOffsetArray = std::array<size_t, 3>;

struct MapInfo {
    MapInfo() = default;
    MapInfo(void *ptr, size_t ptrLength, MemObjSizeArray size, MemObjOffsetArray offset, uint32_t mipLevel = 0)
        : size(size), offset(offset), ptrLength(ptrLength), ptr(ptr), mipLevel(mipLevel) {
    }

    MemObjSizeArray size = {};
    MemObjOffsetArray offset = {};
    size_t ptrLength = 0;
    void *ptr = nullptr;
    bool readOnly = false;
    uint32_t mipLevel = 0;
};

} // namespace LEO
} // namespace NEO
