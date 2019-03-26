/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "mock_gmm_memory.h"

namespace NEO {
GmmMemory *GmmMemory::create() {
    return new MockGmmMemory();
}
} // namespace NEO
