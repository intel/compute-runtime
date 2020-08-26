/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/aub_mapper.h"
#include "shared/source/helpers/flat_batch_buffer_helper_hw.inl"
#include "shared/source/helpers/hw_helper_bdw_plus.inl"

namespace NEO {
typedef SKLFamily Family;

template <>
SipKernelType HwHelperHw<Family>::getSipKernelType(bool debuggingActive) {
    if (!debuggingActive) {
        return SipKernelType::Csr;
    }
    return SipKernelType::DbgCsrLocal;
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

template class HwHelperHw<Family>;
template class FlatBatchBufferHelperHw<Family>;
template struct MemorySynchronizationCommands<Family>;
template struct LriHelper<Family>;
} // namespace NEO
