/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/flat_batch_buffer_helper_hw.inl"
#include "shared/source/helpers/hw_helper_bdw_plus.inl"
#include "opencl/source/aub/aub_helper_bdw_plus.inl"

namespace NEO {
typedef ICLFamily Family;

template <>
uint32_t HwHelperHw<Family>::getComputeUnitsUsedForScratch(const HardwareInfo *pHwInfo) const {
    return pHwInfo->gtSystemInfo.MaxSubSlicesSupported * pHwInfo->gtSystemInfo.MaxEuPerSubSlice * 8;
}

template <>
std::string HwHelperHw<Family>::getExtensions() const {
    return "cl_intel_subgroup_local_block_io ";
}

template class AubHelperHw<Family>;
template class HwHelperHw<Family>;
template class FlatBatchBufferHelperHw<Family>;
template struct MemorySynchronizationCommands<Family>;
} // namespace NEO
