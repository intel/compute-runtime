/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/constants.h"

namespace NEO {

struct SmallBuffersParams {
    size_t aggregatedSmallBuffersPoolSize{0};
    size_t smallBufferThreshold{0};
    size_t chunkAlignment{0};
    size_t startingOffset{0};

    static SmallBuffersParams getDefaultParams();
};

} // namespace NEO
