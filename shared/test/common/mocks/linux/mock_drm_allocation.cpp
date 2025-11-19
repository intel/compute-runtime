/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/linux/mock_drm_allocation.h"

#include "shared/source/gmm_helper/gmm.h"

namespace NEO {

MockDrmAllocation::~MockDrmAllocation() {
    for (uint32_t i = 0; i < getNumGmms(); i++) {
        delete getGmm(i);
    }
    gmms.resize(0);
}

} // namespace NEO
