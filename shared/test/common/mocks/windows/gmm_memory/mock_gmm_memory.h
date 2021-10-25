/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/mocks/windows/mock_gmm_memory_base.h"

namespace NEO {

class MockGmmMemory : public MockGmmMemoryBase {
    using MockGmmMemoryBase::MockGmmMemoryBase;
};
} // namespace NEO
