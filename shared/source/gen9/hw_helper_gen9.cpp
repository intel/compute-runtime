/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/aub_mapper.h"
#include "shared/source/helpers/flat_batch_buffer_helper_hw.inl"
#include "shared/source/helpers/hw_helper_base.inl"
#include "shared/source/helpers/hw_helper_bdw_and_later.inl"
#include "shared/source/helpers/hw_helper_bdw_to_icllp.inl"

#include <cstring>

namespace NEO {
typedef SKLFamily Family;

template <>
SipKernelType HwHelperHw<Family>::getSipKernelType(bool debuggingActive) const {
    if (!debuggingActive) {
        return SipKernelType::Csr;
    }
    return DebugManager.flags.UseBindlessDebugSip.get() ? SipKernelType::DbgBindless : SipKernelType::DbgCsrLocal;
}

template <>
void MemorySynchronizationCommands<Family>::addPipeControlWA(LinearStream &commandStream, uint64_t gpuAddress, const HardwareInfo &hwInfo) {
    using PIPE_CONTROL = typename Family::PIPE_CONTROL;
    PIPE_CONTROL cmd = Family::cmdInitPipeControl;
    cmd.setCommandStreamerStallEnable(true);

    auto pipeControl = static_cast<Family::PIPE_CONTROL *>(commandStream.getSpace(sizeof(Family::PIPE_CONTROL)));
    *pipeControl = cmd;
}

template <>
uint32_t HwHelperHw<Family>::getMetricsLibraryGenId() const {
    return static_cast<uint32_t>(MetricsLibraryApi::ClientGen::Gen9);
}

template <>
uint32_t HwHelperHw<Family>::getDefaultThreadArbitrationPolicy() const {
    return ThreadArbitrationPolicy::RoundRobin;
}

template <>
uint32_t HwHelperHw<Family>::getDefaultRevisionId(const HardwareInfo &hwInfo) const {
    if (std::strcmp(hwInfo.capabilityTable.platformType, "core") == 0) {
        return 9u;
    }
    return 0u;
}

template <>
bool MemorySynchronizationCommands<Family>::isPipeControlWArequired(const HardwareInfo &hwInfo) { return true; }

template class HwHelperHw<Family>;
template class FlatBatchBufferHelperHw<Family>;
template struct MemorySynchronizationCommands<Family>;
template struct LriHelper<Family>;
} // namespace NEO
