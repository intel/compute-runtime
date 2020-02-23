/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/test/unit_test/mocks/mock_gmm_memory_base.h"

namespace NEO {

class MockGmmMemory : public MockGmmMemoryBase {
    using MockGmmMemoryBase::MockGmmMemoryBase;
};

class GmockGmmMemory : public GmockGmmMemoryBase {
    using GmockGmmMemoryBase::GmockGmmMemoryBase;
};
} // namespace NEO
