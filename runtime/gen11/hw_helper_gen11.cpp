/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/aub/aub_helper.h"
#include "runtime/aub/aub_helper.inl"
#include "runtime/helpers/flat_batch_buffer_helper_hw.inl"
#include "runtime/helpers/hw_helper.h"
#include "runtime/helpers/hw_helper_common.inl"

namespace NEO {
typedef ICLFamily Family;

template <>
uint32_t HwHelperHw<Family>::getComputeUnitsUsedForScratch(const HardwareInfo *pHwInfo) const {
    return pHwInfo->gtSystemInfo.MaxSubSlicesSupported * pHwInfo->gtSystemInfo.MaxEuPerSubSlice * 8;
}

template class AubHelperHw<Family>;
template class HwHelperHw<Family>;
template class FlatBatchBufferHelperHw<Family>;
template struct PipeControlHelper<Family>;
} // namespace NEO
