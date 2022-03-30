/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_container/memory_fence_encoder.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/direct_submission/direct_submission_hw.h"

namespace NEO {

template <typename GfxFamily, typename Dispatcher>
inline void DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchPartitionRegisterConfiguration() {
    ImplicitScalingDispatch<GfxFamily>::dispatchRegisterConfiguration(ringCommandStream,
                                                                      this->workPartitionAllocation->getGpuAddress(),
                                                                      this->postSyncOffset);

    if constexpr (GfxFamily::isUsingMiMemFence) {
        if (DebugManager.flags.DirectSubmissionInsertExtraMiMemFenceCommands.get() == 1) {
            auto &engineControl = device.getEngine(this->osContext.getEngineType(), this->osContext.getEngineUsage());

            UNRECOVERABLE_IF(engineControl.osContext->getContextId() != engineControl.osContext->getContextId());

            EncodeMemoryFence<GfxFamily>::encodeSystemMemoryFence(ringCommandStream, engineControl.commandStreamReceiver->getGlobalFenceAllocation());
        }
    }
}

template <typename GfxFamily, typename Dispatcher>
inline size_t DirectSubmissionHw<GfxFamily, Dispatcher>::getSizePartitionRegisterConfigurationSection() {
    auto size = ImplicitScalingDispatch<GfxFamily>::getRegisterConfigurationSize();

    if constexpr (GfxFamily::isUsingMiMemFence) {
        if (DebugManager.flags.DirectSubmissionInsertExtraMiMemFenceCommands.get() == 1) {
            size += EncodeMemoryFence<GfxFamily>::getSystemMemoryFenceSize();
        }
    }

    return size;
}

template <typename GfxFamily, typename Dispatcher>
inline void DirectSubmissionHw<GfxFamily, Dispatcher>::setPostSyncOffset() {
    this->postSyncOffset = ImplicitScalingDispatch<GfxFamily>::getPostSyncOffset();
}

} // namespace NEO
