/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/thread_arbitration_policy.h"
#include "shared/test/common/helpers/unit_test_helper.h"

namespace NEO {

template <typename GfxFamily>
uint64_t UnitTestHelper<GfxFamily>::getAtomicMemoryAddress(const typename GfxFamily::MI_ATOMIC &atomic) {
    return atomic.getMemoryAddress();
}

template <typename GfxFamily>
uint32_t UnitTestHelper<GfxFamily>::getAppropriateThreadArbitrationPolicy(int32_t policy) {
    using STATE_COMPUTE_MODE = typename GfxFamily::STATE_COMPUTE_MODE;
    switch (policy) {
    case ThreadArbitrationPolicy::RoundRobin:
        return STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE::EU_THREAD_SCHEDULING_MODE_ROUND_ROBIN;
    case ThreadArbitrationPolicy::AgeBased:
        return STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE::EU_THREAD_SCHEDULING_MODE_OLDEST_FIRST;
    case ThreadArbitrationPolicy::RoundRobinAfterDependency:
        return STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE::EU_THREAD_SCHEDULING_MODE_STALL_BASED_ROUND_ROBIN;
    default:
        return STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE::EU_THREAD_SCHEDULING_MODE_HW_DEFAULT;
    }
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::isAdditionalSynchronizationRequired() {
    return true;
}

template <typename GfxFamily>
void UnitTestHelper<GfxFamily>::skipStatePrefetch(GenCmdList::iterator &iter) {
    while (genCmdCast<typename GfxFamily::STATE_PREFETCH *>(*iter)) {
        iter++;
    }
}

} // namespace NEO
