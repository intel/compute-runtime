/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "mock_gmm_memory.h"

namespace OCLRT {
GmmMemory *GmmMemory::create() {
    return new MockGmmMemory();
}
} // namespace OCLRT
