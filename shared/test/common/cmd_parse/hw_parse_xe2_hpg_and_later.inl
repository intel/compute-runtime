/*
 * Copyright (C) 2024-2025 Intel Corporation
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
bool HardwareParse::isStallingBarrier<GenGfxFamily>(GenCmdList::iterator &iter) {
    GenGfxFamily::RESOURCE_BARRIER *resourceBarrierCmd = genCmdCast<GenGfxFamily::RESOURCE_BARRIER *>(*iter);
    EXPECT_EQ(resourceBarrierCmd->getBarrierType(), RESOURCE_BARRIER::BARRIER_TYPE::BARRIER_TYPE_IMMEDIATE);
    EXPECT_EQ(resourceBarrierCmd->getWaitStage(), RESOURCE_BARRIER::WAIT_STAGE::WAIT_STAGE_TOP);
    EXPECT_EQ(resourceBarrierCmd->getSignalStage(), RESOURCE_BARRIER::SIGNAL_STAGE::SIGNAL_STAGE_GPGPU);
    return resourceBarrierCmd != nullptr;
}

} // namespace NEO
