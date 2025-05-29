/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/utilities/stackvec.h"
#include "shared/source/utilities/tag_allocator.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"

#include "copy_offload_mode.h"

namespace L0 {

template <typename Family>
L0::Event *L0GfxCoreHelperHw<Family>::createEvent(L0::EventPool *eventPool, const ze_event_desc_t *desc, L0::Device *device) const {
    if (NEO::debugManager.flags.OverrideTimestampPacketSize.get() != -1) {
        if (NEO::debugManager.flags.OverrideTimestampPacketSize.get() == 4) {
            return Event::create<uint32_t>(eventPool, desc, device);
        } else if (NEO::debugManager.flags.OverrideTimestampPacketSize.get() == 8) {
            return Event::create<uint64_t>(eventPool, desc, device);
        } else {
            UNRECOVERABLE_IF(true);
        }
    }

    return Event::create<typename Family::TimestampPacketType>(eventPool, desc, device);
}

template <typename Family>
L0::Event *L0GfxCoreHelperHw<Family>::createStandaloneEvent(const EventDescriptor &desc, L0::Device *device, ze_result_t &result) const {
    return Event::create<typename Family::TimestampPacketType>(desc, device, result);
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::alwaysAllocateEventInLocalMem() const {
    return false;
}

template <typename Family>
uint32_t L0GfxCoreHelperHw<Family>::getCmdListWaitOnMemoryDataSize() const {
    if constexpr (Family::isQwordInOrderCounter) {
        return sizeof(uint64_t);
    } else {
        return sizeof(uint32_t);
    }
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::hasUnifiedPostSyncAllocationLayout() const {
    return (getImmediateWritePostSyncOffset() == NEO::ImplicitScalingDispatch<Family>::getTimeStampPostSyncOffset());
}

template <typename Family>
uint32_t L0GfxCoreHelperHw<Family>::getImmediateWritePostSyncOffset() const {
    return NEO::ImplicitScalingDispatch<Family>::getImmediateWritePostSyncOffset();
}

template <typename Family>
void L0GfxCoreHelperHw<Family>::appendPlatformSpecificExtensions(std::vector<std::pair<std::string, uint32_t>> &extensions, const NEO::ProductHelper &productHelper, const NEO::HardwareInfo &hwInfo) const {
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::implicitSynchronizedDispatchForCooperativeKernelsAllowed() const {
    return false;
}

template <typename Family>
std::unique_ptr<NEO::TagAllocatorBase> L0GfxCoreHelperHw<Family>::getInOrderTimestampAllocator(const RootDeviceIndicesContainer &rootDeviceIndices, NEO::MemoryManager *memoryManager, size_t initialTagCount, size_t packetsCountPerElement,
                                                                                               size_t tagAlignment, NEO::DeviceBitfield deviceBitfield) const {

    using TimestampPacketType = typename Family::TimestampPacketType;
    using TimestampPacketsT = NEO::TimestampPackets<TimestampPacketType, 1>;

    size_t size = sizeof(TimestampPacketsT) * packetsCountPerElement;

    return std::make_unique<NEO::TagAllocator<TimestampPacketsT>>(rootDeviceIndices, memoryManager, initialTagCount, tagAlignment, size, Event::State::STATE_CLEARED, false, true, deviceBitfield);
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::isThreadControlStoppedSupported() const {
    return true;
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::threadResumeRequiresUnlock() const {
    return false;
}

template <typename Family>
CopyOffloadMode L0GfxCoreHelperHw<Family>::getDefaultCopyOffloadMode(bool additionalBlitPropertiesSupported) const {
    if (NEO::debugManager.flags.OverrideCopyOffloadMode.get() != -1) {
        return static_cast<CopyOffloadMode>(NEO::debugManager.flags.OverrideCopyOffloadMode.get());
    }

    return CopyOffloadModes::dualStream;
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::isDefaultCmdListWithCopyOffloadSupported(bool additionalBlitPropertiesSupported) const {
    return (NEO::debugManager.flags.ForceCopyOperationOffloadForComputeCmdList.get() == 2);
}

} // namespace L0
