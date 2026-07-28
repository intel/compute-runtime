/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/unit_test_helper.h"

namespace NEO {

template <typename GfxFamily>
uint64_t UnitTestHelper<GfxFamily>::getSemaphoreWaitAddress(const void *semWaitCmd) {
    using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;
    return reinterpret_cast<const MI_SEMAPHORE_WAIT *>(semWaitCmd)->getSemaphoreGraphicsAddress();
}

template <typename GfxFamily>
uint64_t UnitTestHelper<GfxFamily>::getSemaphoreWaitData(const void *semWaitCmd) {
    using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;
    return reinterpret_cast<const MI_SEMAPHORE_WAIT *>(semWaitCmd)->getSemaphoreDataDword();
}

} // namespace NEO
