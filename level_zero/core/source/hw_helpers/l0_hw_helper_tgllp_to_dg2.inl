/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"

#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"

namespace L0 {

template <typename Family>
void L0GfxCoreHelperHw<Family>::setAdditionalGroupProperty(ze_command_queue_group_properties_t &groupProperty, NEO::EngineGroupT &group) const {
}

template <typename Family>
void L0GfxCoreHelperHw<Family>::getAttentionBitmaskForSingleThreads(const std::vector<EuThread::ThreadId> &threads, const NEO::HardwareInfo &hwInfo, std::unique_ptr<uint8_t[]> &bitmask, size_t &bitmaskSize) const {

    const uint32_t numSubslicesPerSlice = (hwInfo.gtSystemInfo.MaxEuPerSubSlice == 8) ? hwInfo.gtSystemInfo.MaxDualSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported : hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    const uint32_t numThreadsPerEu = (hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount);
    const uint32_t bytesPerEu = alignUp(numThreadsPerEu, 8) / 8;
    const uint32_t numEuPerSubslice = std::min(hwInfo.gtSystemInfo.MaxEuPerSubSlice, 8u);
    const uint32_t threadsSizePerSlice = numSubslicesPerSlice * numEuPerSubslice * bytesPerEu;
    const uint32_t highestEnabledSlice = NEO::GfxCoreHelper::getHighestEnabledSlice(hwInfo);

    const uint32_t eusPerRow = 4;
    const uint32_t numberOfRows = 2;

    bitmaskSize = std::max(highestEnabledSlice, hwInfo.gtSystemInfo.MaxSlicesSupported) * numSubslicesPerSlice * numEuPerSubslice * bytesPerEu;
    bitmask = std::make_unique<uint8_t[]>(bitmaskSize);

    memset(bitmask.get(), 0, bitmaskSize);

    for (auto &thread : threads) {
        uint8_t *sliceData = ptrOffset(bitmask.get(), threadsSizePerSlice * thread.slice);
        uint8_t *euData;

        if (hwInfo.gtSystemInfo.MaxEuPerSubSlice == 8) {
            uint8_t *subsliceData = ptrOffset(sliceData, numEuPerSubslice * bytesPerEu * (thread.subslice / 2));
            auto eu = thread.eu % eusPerRow;
            auto dualEu = thread.subslice % 2;
            euData = ptrOffset(subsliceData, bytesPerEu * (eu + dualEu * eusPerRow));
        } else {
            uint8_t *subsliceData = ptrOffset(sliceData, numEuPerSubslice * bytesPerEu * thread.subslice);
            auto eu = thread.eu % eusPerRow;
            auto dualEu = thread.eu / (numberOfRows * eusPerRow);
            euData = ptrOffset(subsliceData, bytesPerEu * (eu + dualEu * eusPerRow));
        }

        UNRECOVERABLE_IF(thread.thread > 7);
        *euData |= (1 << thread.thread);
    }
}

template <typename Family>
std::vector<EuThread::ThreadId> L0GfxCoreHelperHw<Family>::getThreadsFromAttentionBitmask(const NEO::HardwareInfo &hwInfo, uint32_t tile, const uint8_t *bitmask, const size_t bitmaskSize) const {

    const uint32_t numSubslicesPerSlice = (hwInfo.gtSystemInfo.MaxEuPerSubSlice == 8) ? hwInfo.gtSystemInfo.MaxDualSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported : hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    const uint32_t numEuPerSubslice = std::min(hwInfo.gtSystemInfo.MaxEuPerSubSlice, 8u);
    const uint32_t numThreadsPerEu = (hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount);
    const uint32_t bytesPerEu = alignUp(numThreadsPerEu, 8) / 8;
    const uint32_t threadsSizePerSlice = numSubslicesPerSlice * numEuPerSubslice * bytesPerEu;
    const uint32_t threadsSizePerSubSlice = numEuPerSubslice * bytesPerEu;
    const uint32_t eusPerRow = 4;
    const uint32_t numberOfRows = 2;
    const uint32_t highestEnabledSlice = NEO::GfxCoreHelper::getHighestEnabledSlice(hwInfo);

    UNRECOVERABLE_IF(bytesPerEu != 1);
    std::vector<EuThread::ThreadId> threads;

    for (uint32_t slice = 0; slice < std::max(highestEnabledSlice, hwInfo.gtSystemInfo.MaxSlicesSupported); slice++) {
        for (uint32_t subslice = 0; subslice < numSubslicesPerSlice; subslice++) {

            size_t subSliceOffset = slice * threadsSizePerSlice + subslice * threadsSizePerSubSlice;

            for (uint32_t dualEu = 0; dualEu < numberOfRows; dualEu++) {
                for (uint32_t euIndex = 0; euIndex < eusPerRow; euIndex++) {

                    auto offset = subSliceOffset + euIndex + dualEu * eusPerRow;

                    if (offset >= bitmaskSize) {
                        return threads;
                    }

                    std::bitset<8> bits(bitmask[offset]);
                    for (uint32_t i = 0; i < 8; i++) {
                        if (bits.test(i)) {
                            if (hwInfo.gtSystemInfo.MaxEuPerSubSlice == 8) {
                                threads.emplace_back(tile, slice, (subslice * 2) + dualEu, euIndex, i);
                                threads.emplace_back(tile, slice, (subslice * 2) + dualEu, euIndex + eusPerRow, i);
                            } else {
                                threads.emplace_back(tile, slice, subslice, euIndex + numEuPerSubslice * dualEu, i);
                                threads.emplace_back(tile, slice, subslice, euIndex + eusPerRow + numEuPerSubslice * dualEu, i);
                            }
                        }
                    }
                }
            }
        }
    }

    return threads;
}

} // namespace L0
