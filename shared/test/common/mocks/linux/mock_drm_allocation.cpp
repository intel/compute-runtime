/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/linux/mock_drm_allocation.h"

#include "shared/source/gmm_helper/gmm.h"

namespace NEO {

MockDrmAllocation::~MockDrmAllocation() {
    for (auto gmm : gmms) {
        delete gmm;
    }
    gmms.resize(0);
}

} // namespace NEO
