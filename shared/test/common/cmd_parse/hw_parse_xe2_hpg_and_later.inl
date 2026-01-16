/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/hw_parse.h"

namespace NEO {

template <>
void HardwareParse::findCsrBaseAddress<GenGfxFamily>() {
    using STATE_CONTEXT_DATA_BASE_ADDRESS = typename GenGfxFamily::STATE_CONTEXT_DATA_BASE_ADDRESS;
    itorGpgpuCsrBaseAddress = find<STATE_CONTEXT_DATA_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    if (itorGpgpuCsrBaseAddress != cmdList.end()) {
        cmdGpgpuCsrBaseAddress = *itorGpgpuCsrBaseAddress;
    }
}

template <>
bool HardwareParse::requiresPipelineSelectBeforeMediaState<GenGfxFamily>() {
    return false;
}

template <>
void HardwareParse::verifyL1FlushOnStallingBarrier<GenGfxFamily>(bool expectInvalidate, bool expectFlush) {
    if constexpr (std::is_same_v<GenGfxFamily::StallingBarrierType, GenGfxFamily::RESOURCE_BARRIER>) {
        auto itorResourceBarrier = find<GenGfxFamily::RESOURCE_BARRIER *>(cmdList.begin(), cmdList.end());
        auto resourceBarrierCmd = genCmdCast<GenGfxFamily::RESOURCE_BARRIER *>(*itorResourceBarrier);
        ASSERT_NE(itorResourceBarrier, cmdList.end());
        EXPECT_EQ(resourceBarrierCmd->getL1DataportUavFlush(), expectFlush);
        EXPECT_EQ(resourceBarrierCmd->getL1DataportCacheInvalidate(), expectInvalidate);
    }
}

} // namespace NEO
