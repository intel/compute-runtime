/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/aub/aub_helper.h"
#include "runtime/aub/aub_helper.inl"
#include "runtime/helpers/flat_batch_buffer_helper_hw.inl"
#include "runtime/helpers/hw_helper.h"
#include "runtime/helpers/hw_helper_common.inl"

namespace OCLRT {
typedef SKLFamily Family;

template <>
bool HwHelperHw<Family>::setupPreemptionRegisters(HardwareInfo *pHwInfo, bool enable) {
    pHwInfo->capabilityTable.whitelistedRegisters.csChicken1_0x2580 = enable;
    return pHwInfo->capabilityTable.whitelistedRegisters.csChicken1_0x2580;
}

template <>
SipKernelType HwHelperHw<Family>::getSipKernelType(bool debuggingActive) {
    if (!debuggingActive) {
        return SipKernelType::Csr;
    }
    return SipKernelType::DbgCsrLocal;
}

template class AubHelperHw<Family>;
template class HwHelperHw<Family>;
template class FlatBatchBufferHelperHw<Family>;
} // namespace OCLRT
