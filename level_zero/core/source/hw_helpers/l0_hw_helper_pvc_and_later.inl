/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/definitions/engine_group_types.h"
#include "shared/source/helpers/engine_node_helper.h"

#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"

#include <limits>

namespace L0 {

template <typename Family>
void L0GfxCoreHelperHw<Family>::getAttentionBitmaskForSingleThreads(const std::vector<EuThread::ThreadId> &threads, const NEO::HardwareInfo &hwInfo, std::unique_ptr<uint8_t[]> &bitmask, size_t &bitmaskSize) const {
    const uint32_t numSubslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    const uint32_t numEuPerSubslice = hwInfo.gtSystemInfo.MaxEuPerSubSlice;
    const uint32_t numThreadsPerEu = (hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount);
    const uint32_t bytesPerEu = alignUp(numThreadsPerEu, 8) / 8;
    const uint32_t threadsSizePerSlice = numSubslicesPerSlice * numEuPerSubslice * bytesPerEu;
    const uint32_t highestEnabledSlice = NEO::GfxCoreHelper::getHighestEnabledSlice(hwInfo);

    bitmaskSize = std::max(highestEnabledSlice, hwInfo.gtSystemInfo.MaxSlicesSupported) * numSubslicesPerSlice * numEuPerSubslice * bytesPerEu;
    bitmask = std::make_unique<uint8_t[]>(bitmaskSize);

    memset(bitmask.get(), 0, bitmaskSize);

    for (auto &thread : threads) {
        uint8_t *sliceData = ptrOffset(bitmask.get(), threadsSizePerSlice * thread.slice);
        uint8_t *subsliceData = ptrOffset(sliceData, numEuPerSubslice * bytesPerEu * thread.subslice);
        uint8_t *euData = ptrOffset(subsliceData, bytesPerEu * thread.eu);
        UNRECOVERABLE_IF(thread.thread > 7);
        *euData |= (1 << thread.thread);
    }
}

template <typename Family>
std::vector<EuThread::ThreadId> L0GfxCoreHelperHw<Family>::getThreadsFromAttentionBitmask(const NEO::HardwareInfo &hwInfo, uint32_t tile, const uint8_t *bitmask, const size_t bitmaskSize) const {
    const uint32_t numSubslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    const uint32_t numEuPerSubslice = hwInfo.gtSystemInfo.MaxEuPerSubSlice;
    const uint32_t numThreadsPerEu = (hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount);
    const uint32_t bytesPerEu = alignUp(numThreadsPerEu, 8) / 8;
    const uint32_t threadsSizePerSlice = numSubslicesPerSlice * numEuPerSubslice * bytesPerEu;
    const uint32_t threadsSizePerSubSlice = numEuPerSubslice * bytesPerEu;
    const uint32_t highestEnabledSlice = NEO::GfxCoreHelper::getHighestEnabledSlice(hwInfo);

    UNRECOVERABLE_IF(bytesPerEu != 1);
    std::vector<EuThread::ThreadId> threads;

    for (uint32_t slice = 0; slice < std::max(highestEnabledSlice, hwInfo.gtSystemInfo.MaxSlicesSupported); slice++) {
        for (uint32_t subslice = 0; subslice < numSubslicesPerSlice; subslice++) {
            for (uint32_t eu = 0; eu < hwInfo.gtSystemInfo.MaxEuPerSubSlice; eu++) {
                size_t offset = slice * threadsSizePerSlice + subslice * threadsSizePerSubSlice + eu * bytesPerEu;

                if (offset >= bitmaskSize) {
                    return threads;
                }

                std::bitset<8> bits(bitmask[offset]);
                for (uint32_t i = 0; i < 8; i++) {
                    if (bits.test(i)) {
                        threads.emplace_back(tile, slice, subslice, eu, i);
                    }
                }
            }
        }
    }

    return threads;
}

template <typename Family>
void L0GfxCoreHelperHw<Family>::setAdditionalGroupProperty(ze_command_queue_group_properties_t &groupProperty, NEO::EngineGroupT &group) const {
    if (group.engineGroupType == NEO::EngineGroupType::LinkedCopy) {
        groupProperty.flags = ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY;
        groupProperty.maxMemoryFillPatternSize = sizeof(uint8_t);
    }

    if (group.engineGroupType == NEO::EngineGroupType::Copy) {
        groupProperty.flags = ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY;

        bool virtualEnginesEnabled = true;
        for (const auto &engine : group.engines) {
            if (engine.osContext) {
                virtualEnginesEnabled &= NEO::EngineHelpers::isBcsVirtualEngineEnabled(engine.getEngineType());
            }
        }
        if (virtualEnginesEnabled) {
            groupProperty.maxMemoryFillPatternSize = sizeof(uint8_t);
        }
    }
}

} // namespace L0
