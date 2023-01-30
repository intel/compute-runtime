/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/aub_mapper.h"
#include "shared/source/gen11/hw_cmds_base.h"
#include "shared/source/helpers/flat_batch_buffer_helper_hw.inl"
#include "shared/source/helpers/hw_helper_base.inl"
#include "shared/source/helpers/hw_helper_bdw_and_later.inl"
#include "shared/source/helpers/hw_helper_bdw_to_icllp.inl"
#include "shared/source/helpers/logical_state_helper.inl"

namespace NEO {
typedef Gen11Family Family;

template <>
uint32_t GfxCoreHelperHw<Family>::getComputeUnitsUsedForScratch(const RootDeviceEnvironment &rootDeviceEnvironment) const {
    auto hwInfo = rootDeviceEnvironment.getHardwareInfo();
    return hwInfo->gtSystemInfo.MaxSubSlicesSupported * hwInfo->gtSystemInfo.MaxEuPerSubSlice * 8;
}

template <>
std::string GfxCoreHelperHw<Family>::getExtensions(const RootDeviceEnvironment &rootDeviceEnvironment) const {
    return "cl_intel_subgroup_local_block_io ";
}

template <>
int32_t GfxCoreHelperHw<Family>::getDefaultThreadArbitrationPolicy() const {
    return ThreadArbitrationPolicy::RoundRobinAfterDependency;
}

template <>
bool GfxCoreHelperHw<Family>::packedFormatsSupported() const {
    return true;
}

template class GfxCoreHelperHw<Family>;
template class FlatBatchBufferHelperHw<Family>;
template struct MemorySynchronizationCommands<Family>;
template struct LriHelper<Family>;

template LogicalStateHelper *LogicalStateHelper::create<Family>();
} // namespace NEO
