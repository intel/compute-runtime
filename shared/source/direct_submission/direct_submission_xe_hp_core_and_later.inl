/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/direct_submission/direct_submission_hw.h"

namespace NEO {

template <typename GfxFamily, typename Dispatcher>
inline void DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchPartitionRegisterConfiguration() {
    ImplicitScalingDispatch<GfxFamily>::dispatchRegisterConfiguration(ringCommandStream,
                                                                      this->workPartitionAllocation->getGpuAddress(),
                                                                      CommonConstants::partitionAddressOffset);
}

template <typename GfxFamily, typename Dispatcher>
inline size_t DirectSubmissionHw<GfxFamily, Dispatcher>::getSizePartitionRegisterConfigurationSection() {
    return ImplicitScalingDispatch<GfxFamily>::getRegisterConfigurationSize();
}

} // namespace NEO
