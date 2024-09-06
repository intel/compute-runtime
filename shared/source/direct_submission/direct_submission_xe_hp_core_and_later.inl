/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/direct_submission/direct_submission_hw.h"

namespace NEO {

template <typename GfxFamily, typename Dispatcher>
inline void DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchPartitionRegisterConfiguration() {
    ImplicitScalingDispatch<GfxFamily>::dispatchRegisterConfiguration(ringCommandStream,
                                                                      this->workPartitionAllocation->getGpuAddress(),
                                                                      this->immWritePostSyncOffset,
                                                                      Dispatcher::isCopy());
}

template <typename GfxFamily, typename Dispatcher>
inline size_t DirectSubmissionHw<GfxFamily, Dispatcher>::getSizePartitionRegisterConfigurationSection() {
    return ImplicitScalingDispatch<GfxFamily>::getRegisterConfigurationSize();
}

template <typename GfxFamily, typename Dispatcher>
inline void DirectSubmissionHw<GfxFamily, Dispatcher>::setImmWritePostSyncOffset() {
    this->immWritePostSyncOffset = ImplicitScalingDispatch<GfxFamily>::getImmediateWritePostSyncOffset();
}

} // namespace NEO
