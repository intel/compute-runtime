/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/unit_test_helper.h"

namespace NEO {

template <typename GfxFamily>
uint64_t UnitTestHelper<GfxFamily>::getSemaphoreWaitAddress(void *semWaitCmd, bool useSem64) {
    using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;
    using MI_SEMAPHORE_WAIT_LEGACY = typename GfxFamily::MI_SEMAPHORE_WAIT_LEGACY;
    if (useSem64) {
        return reinterpret_cast<MI_SEMAPHORE_WAIT *>(semWaitCmd)->getSemaphoreGraphicsAddress();
    } else {
        return reinterpret_cast<MI_SEMAPHORE_WAIT_LEGACY *>(semWaitCmd)->getSemaphoreGraphicsAddress();
    }
}

} // namespace NEO
