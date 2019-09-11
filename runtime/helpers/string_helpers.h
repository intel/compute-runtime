/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/utilities/stackvec.h"

#include "CL/cl.h"

#include <cstdint>
#include <cstring>
#include <string>

const int maximalStackSizeSizes = 16;

inline int createCombinedString(
    std::string &dstString,
    size_t &dstStringSizeInBytes,
    uint32_t numStrings,
    const char **strings,
    const size_t *lengths) {
    int retVal = CL_SUCCESS;

    if (numStrings == 0 || strings == nullptr) {
        retVal = CL_INVALID_VALUE;
    }

    using SourceSizesT = StackVec<size_t, maximalStackSizeSizes>;
    SourceSizesT localSizes;

    if (retVal == CL_SUCCESS) {
        localSizes.resize(numStrings);
        dstStringSizeInBytes = 1;
        for (uint32_t i = 0; i < numStrings; i++) {
            if (strings[i] == nullptr) {
                retVal = CL_INVALID_VALUE;
                break;
            }
            if ((lengths == nullptr) ||
                (lengths[i] == 0)) {
                localSizes[i] = strlen((const char *)strings[i]);
            } else {
                localSizes[i] = lengths[i];
            }

            dstStringSizeInBytes += localSizes[i];
        }
    }

    if (retVal == CL_SUCCESS) {
        dstString.reserve(dstStringSizeInBytes);
        for (uint32_t i = 0; i < numStrings; i++) {
            dstString.append(strings[i], localSizes[i]);
        }
        // add the null terminator
        dstString += '\0';
    }

    return retVal;
}
