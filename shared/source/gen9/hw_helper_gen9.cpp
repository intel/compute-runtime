/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/aub_mapper.h"
#include "shared/source/gen9/hw_cmds_base.h"
#include "shared/source/helpers/flat_batch_buffer_helper_hw.inl"
#include "shared/source/helpers/hw_helper_base.inl"
#include "shared/source/helpers/hw_helper_bdw_and_later.inl"
#include "shared/source/helpers/hw_helper_bdw_to_icllp.inl"
#include "shared/source/helpers/logical_state_helper.inl"

#include <cstring>

namespace NEO {
typedef Gen9Family Family;

template <>
SipKernelType GfxCoreHelperHw<Family>::getSipKernelType(bool debuggingActive) const {
    if (!debuggingActive) {
        return SipKernelType::Csr;
    }
    return DebugManager.flags.UseBindlessDebugSip.get() ? SipKernelType::DbgBindless : SipKernelType::DbgCsrLocal;
}

template <>
uint32_t GfxCoreHelperHw<Family>::getMetricsLibraryGenId() const {
    return static_cast<uint32_t>(MetricsLibraryApi::ClientGen::Gen9);
}

template <>
int32_t GfxCoreHelperHw<Family>::getDefaultThreadArbitrationPolicy() const {
    return ThreadArbitrationPolicy::RoundRobin;
}

template <>
bool MemorySynchronizationCommands<Family>::isBarrierWaRequired(const RootDeviceEnvironment &rootDeviceEnvironment) { return true; }

template <>
bool GfxCoreHelperHw<Family>::isTimestampShiftRequired() const {
    return false;
}

template class GfxCoreHelperHw<Family>;
template class FlatBatchBufferHelperHw<Family>;
template struct MemorySynchronizationCommands<Family>;
template struct LriHelper<Family>;

template LogicalStateHelper *LogicalStateHelper::create<Family>();
} // namespace NEO
