/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/debug_helpers.h"
#include "shared/test/common/helpers/unit_test_helper.h"

namespace NEO {

namespace {
template <typename GfxFamily>
inline const typename GfxFamily::MI_SEMAPHORE_WAIT_LEGACY *matchLegacySemaphoreWaitCmd(const void *semWaitCmd) {
    using MI_SEMAPHORE_WAIT_LEGACY = typename GfxFamily::MI_SEMAPHORE_WAIT_LEGACY;
    return matchCommandHeader<MI_SEMAPHORE_WAIT_LEGACY>(const_cast<void *>(semWaitCmd), [](const MI_SEMAPHORE_WAIT_LEGACY &header) {
        return header.TheStructure.Common.MiCommandOpcode == static_cast<uint32_t>(MI_SEMAPHORE_WAIT_LEGACY::MI_COMMAND_OPCODE_MI_SEMAPHORE_WAIT);
    });
}

template <typename GfxFamily>
inline const typename GfxFamily::MI_SEMAPHORE_WAIT_64 *matchSemaphoreWait64Cmd(const void *semWaitCmd) {
    using MI_SEMAPHORE_WAIT_64 = typename GfxFamily::MI_SEMAPHORE_WAIT_64;
    return matchCommandHeader<MI_SEMAPHORE_WAIT_64>(const_cast<void *>(semWaitCmd), [](const MI_SEMAPHORE_WAIT_64 &header) {
        return header.TheStructure.Common.MiCommandOpcode == static_cast<uint32_t>(MI_SEMAPHORE_WAIT_64::MI_COMMAND_OPCODE_MI_SEMAPHORE_WAIT);
    });
}
} // namespace

template <typename GfxFamily>
uint64_t UnitTestHelper<GfxFamily>::getSemaphoreWaitAddress(const void *semWaitCmd) {
    if (auto legacyCmd = matchLegacySemaphoreWaitCmd<GfxFamily>(semWaitCmd)) {
        return legacyCmd->getSemaphoreGraphicsAddress();
    }
    if (auto cmd64 = matchSemaphoreWait64Cmd<GfxFamily>(semWaitCmd)) {
        return cmd64->getSemaphoreGraphicsAddress();
    }
    DEBUG_BREAK_IF(true);
    return 0u;
}

template <typename GfxFamily>
uint64_t UnitTestHelper<GfxFamily>::getSemaphoreWaitData(const void *semWaitCmd) {
    if (auto legacyCmd = matchLegacySemaphoreWaitCmd<GfxFamily>(semWaitCmd)) {
        return legacyCmd->getSemaphoreDataDword();
    }
    if (auto cmd64 = matchSemaphoreWait64Cmd<GfxFamily>(semWaitCmd)) {
        return cmd64->getSemaphoreDataDword();
    }
    DEBUG_BREAK_IF(true);
    return 0u;
}

} // namespace NEO
