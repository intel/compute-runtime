/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"

namespace L0 {

template <typename Family>
void L0GfxCoreHelperHw<Family>::getAttentionBitmaskForSingleThreads(const std::vector<EuThread::ThreadId> &threads, const NEO::HardwareInfo &hwInfo, std::unique_ptr<uint8_t[]> &bitmask, size_t &bitmaskSize) const {
    const uint32_t numSubslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    const uint32_t numEuPerSubslice = std::min(hwInfo.gtSystemInfo.MaxEuPerSubSlice, 8u);
    const uint32_t numThreadsPerEu = hwInfo.gtSystemInfo.NumThreadsPerEu;
    const uint32_t bytesPerEu = alignUp(numThreadsPerEu, 8) / 8;
    const uint32_t threadsSizePerSlice = numSubslicesPerSlice * numEuPerSubslice * bytesPerEu;
    const uint32_t highestEnabledSlice = NEO::GfxCoreHelper::getHighestEnabledSlice(hwInfo);

    bitmaskSize = std::max(highestEnabledSlice, hwInfo.gtSystemInfo.MaxSlicesSupported) * numSubslicesPerSlice * numEuPerSubslice * bytesPerEu;
    bitmask = std::make_unique<uint8_t[]>(bitmaskSize);

    memset(bitmask.get(), 0, bitmaskSize);

    for (auto &thread : threads) {
        uint8_t *sliceData = ptrOffset(bitmask.get(), threadsSizePerSlice * thread.slice);

        uint8_t *subsliceData = ptrOffset(sliceData, numEuPerSubslice * bytesPerEu * thread.subslice);
        UNRECOVERABLE_IF(thread.thread > 9);

        auto euByteNum = (thread.thread / 8);
        uint8_t *euData = ptrOffset(subsliceData, euByteNum * numEuPerSubslice + thread.eu);

        *euData |= 1 << ((thread.thread) % 8);
    }
}

template <typename Family>
std::vector<EuThread::ThreadId> L0GfxCoreHelperHw<Family>::getThreadsFromAttentionBitmask(const NEO::HardwareInfo &hwInfo, uint32_t tile, const uint8_t *bitmask, const size_t bitmaskSize) const {
    const uint32_t numSubslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    const uint32_t numEuPerSubslice = hwInfo.gtSystemInfo.MaxEuPerSubSlice;
    const uint32_t numThreadsPerEu = hwInfo.gtSystemInfo.NumThreadsPerEu;

    const uint32_t bytesPerEu = alignUp(numThreadsPerEu, 8) / 8;
    const uint32_t threadsSizePerSlice = numSubslicesPerSlice * numEuPerSubslice * bytesPerEu;
    const uint32_t threadsSizePerSubSlice = numEuPerSubslice * bytesPerEu;
    const uint32_t highestEnabledSlice = NEO::GfxCoreHelper::getHighestEnabledSlice(hwInfo);

    std::vector<EuThread::ThreadId> threads;

    for (uint32_t slice = 0; slice < std::max(highestEnabledSlice, hwInfo.gtSystemInfo.MaxSlicesSupported); slice++) {
        for (uint32_t subslice = 0; subslice < numSubslicesPerSlice; subslice++) {
            for (uint32_t eu = 0; eu < hwInfo.gtSystemInfo.MaxEuPerSubSlice; eu++) {

                size_t offset = slice * threadsSizePerSlice + subslice * threadsSizePerSubSlice + eu * bytesPerEu;

                if (offset >= bitmaskSize) {
                    return threads;
                }

                UNRECOVERABLE_IF(!bitmask);
                for (uint32_t byte = 0; byte < bytesPerEu; byte++) {
                    std::bitset<8> bits(bitmask[offset + byte]);
                    for (uint32_t i = 0; i < 8; i++) {
                        if (bits.test(i)) {
                            threads.emplace_back(tile, slice, subslice, (((eu % (numEuPerSubslice / bytesPerEu)) * bytesPerEu)) + byte, i + 8 * (eu / (numEuPerSubslice / bytesPerEu)));
                        }
                    }
                }
            }
        }
    }

    return threads;
}

template <typename Family>
ze_rtas_format_exp_t L0GfxCoreHelperHw<Family>::getSupportedRTASFormatExp() const {
    return static_cast<ze_rtas_format_exp_t>(RTASDeviceFormatInternal::version2);
}

template <typename Family>
ze_rtas_format_ext_t L0GfxCoreHelperHw<Family>::getSupportedRTASFormatExt() const {
    return static_cast<ze_rtas_format_ext_t>(RTASDeviceFormatInternal::version2);
}

template <typename Family>
zet_debug_regset_type_intel_gpu_t L0GfxCoreHelperHw<Family>::getRegsetTypeForLargeGrfDetection() const {
    return ZET_DEBUG_REGSET_TYPE_SR_INTEL_GPU;
}
} // namespace L0
