/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/ptr_math.h"

#include <cstdint>

using TaskCountType = uint64_t;
using TagAddressType = uint64_t;

namespace NEO {
namespace TaskCountHelper {

inline bool isReady(volatile const TagAddressType *tagAddress, TaskCountType taskCount, size_t partitionCount, size_t partitionOffset) {
    if (!tagAddress) {
        return false;
    }
    for (size_t partition = 0; partition < partitionCount; partition++) {
        if (*tagAddress < taskCount) {
            return false;
        }
        tagAddress = ptrOffset(tagAddress, partitionOffset);
    }
    return true;
}

} // namespace TaskCountHelper
} // namespace NEO
