/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"

namespace L0 {

template <typename GfxFamily>
L0::Event *L0HwHelperHw<GfxFamily>::createEvent(L0::EventPool *eventPool, const ze_event_desc_t *desc, L0::Device *device) const {
    if (NEO::DebugManager.flags.OverrideTimestampPacketSize.get() != -1) {
        if (NEO::DebugManager.flags.OverrideTimestampPacketSize.get() == 4) {
            return Event::create<uint32_t>(eventPool, desc, device);
        } else if (NEO::DebugManager.flags.OverrideTimestampPacketSize.get() == 8) {
            return Event::create<uint64_t>(eventPool, desc, device);
        } else {
            UNRECOVERABLE_IF(true);
        }
    }

    return Event::create<typename GfxFamily::TimestampPacketType>(eventPool, desc, device);
}

template <typename GfxFamily>
bool L0HwHelperHw<GfxFamily>::isResumeWARequired() {
    return false;
}

template <typename GfxFamily>
void L0HwHelperHw<GfxFamily>::getAttentionBitmaskForSingleThreads(const std::vector<EuThread::ThreadId> &threads, const NEO::HardwareInfo &hwInfo, std::unique_ptr<uint8_t[]> &bitmask, size_t &bitmaskSize) const {
    const uint32_t numSubslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    const uint32_t numEuPerSubslice = hwInfo.gtSystemInfo.MaxEuPerSubSlice;
    const uint32_t numThreadsPerEu = (hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount);
    const uint32_t bytesPerEu = alignUp(numThreadsPerEu, 8) / 8;
    const uint32_t threadsSizePerSlice = numSubslicesPerSlice * numEuPerSubslice * bytesPerEu;

    bitmaskSize = hwInfo.gtSystemInfo.MaxSubSlicesSupported * hwInfo.gtSystemInfo.MaxEuPerSubSlice * bytesPerEu;
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

template <typename GfxFamily>
std::vector<EuThread::ThreadId> L0HwHelperHw<GfxFamily>::getThreadsFromAttentionBitmask(const NEO::HardwareInfo &hwInfo, uint32_t tile, const uint8_t *bitmask, const size_t bitmaskSize) const {
    const uint32_t numSubslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    const uint32_t numEuPerSubslice = hwInfo.gtSystemInfo.MaxEuPerSubSlice;
    const uint32_t numThreadsPerEu = (hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount);
    const uint32_t bytesPerEu = alignUp(numThreadsPerEu, 8) / 8;
    const uint32_t threadsSizePerSlice = numSubslicesPerSlice * numEuPerSubslice * bytesPerEu;
    const uint32_t threadsSizePerSubSlice = numEuPerSubslice * bytesPerEu;

    UNRECOVERABLE_IF(bytesPerEu != 1);
    std::vector<EuThread::ThreadId> threads;

    for (uint32_t slice = 0; slice < hwInfo.gtSystemInfo.MaxSlicesSupported; slice++) {
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

template <typename GfxFamily>
bool L0HwHelperHw<GfxFamily>::imageCompressionSupported(const NEO::HardwareInfo &hwInfo) const {
    if (NEO::DebugManager.flags.RenderCompressedImagesEnabled.get() != -1) {
        return !!NEO::DebugManager.flags.RenderCompressedImagesEnabled.get();
    }

    return false;
}

template <typename GfxFamily>
bool L0HwHelperHw<GfxFamily>::usmCompressionSupported(const NEO::HardwareInfo &hwInfo) const {
    if (NEO::DebugManager.flags.RenderCompressedBuffersEnabled.get() != -1) {
        return !!NEO::DebugManager.flags.RenderCompressedBuffersEnabled.get();
    }

    return false;
}

template <typename GfxFamily>
bool L0HwHelperHw<GfxFamily>::forceDefaultUsmCompressionSupport() const {
    return false;
}

template <typename gfxProduct>
bool L0HwHelperHw<gfxProduct>::alwaysAllocateEventInLocalMem() const {
    return false;
}

} // namespace L0
