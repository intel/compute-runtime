/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/hw_helper_bdw_plus.inl"
#include "runtime/aub/aub_helper_bdw_plus.inl"
#include "runtime/helpers/flat_batch_buffer_helper_hw.inl"

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
void PipeControlHelper<Family>::addPipeControlWA(LinearStream &commandStream, const HardwareInfo &hwInfo) {
    auto pCmd = static_cast<Family::PIPE_CONTROL *>(commandStream.getSpace(sizeof(Family::PIPE_CONTROL)));
    *pCmd = Family::cmdInitPipeControl;
    pCmd->setCommandStreamerStallEnable(true);
}

template <>
uint32_t HwHelperHw<Family>::getMetricsLibraryGenId() const {
    return static_cast<uint32_t>(MetricsLibraryApi::ClientGen::Gen9);
}

template class AubHelperHw<Family>;
template class HwHelperHw<Family>;
template class FlatBatchBufferHelperHw<Family>;
template struct PipeControlHelper<Family>;
} // namespace NEO
