/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "mock_gmm_memory.h"

namespace NEO {
GmmMemory *GmmMemory::create(GmmClientContext *gmmClientContext) {
    return new MockGmmMemory(gmmClientContext);
}
} // namespace NEO
