/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/windows/mock_gmm_memory.h"

namespace NEO {
GmmMemory *GmmMemory::create(GmmClientContext *gmmClientContext) {
    return new MockGmmMemory(gmmClientContext);
}
} // namespace NEO
