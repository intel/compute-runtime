/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_helper.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"

namespace L0 {

template <typename Family>
L0::Event *L0HwHelperHw<Family>::createEvent(L0::EventPool *eventPool, const ze_event_desc_t *desc, L0::Device *device) const {
    if (NEO::DebugManager.flags.OverrideTimestampPacketSize.get() != -1) {
        if (NEO::DebugManager.flags.OverrideTimestampPacketSize.get() == 4) {
            return Event::create<uint32_t>(eventPool, desc, device);
        } else if (NEO::DebugManager.flags.OverrideTimestampPacketSize.get() == 8) {
            return Event::create<uint64_t>(eventPool, desc, device);
        } else {
            UNRECOVERABLE_IF(true);
        }
    }

    return Event::create<typename Family::TimestampPacketType>(eventPool, desc, device);
}

template <typename Family>
bool L0HwHelperHw<Family>::isResumeWARequired() {
    return false;
}

template <typename Family>
bool L0HwHelperHw<Family>::imageCompressionSupported(const NEO::HardwareInfo &hwInfo) const {
    if (NEO::DebugManager.flags.RenderCompressedImagesEnabled.get() != -1) {
        return !!NEO::DebugManager.flags.RenderCompressedImagesEnabled.get();
    }

    return false;
}

template <typename Family>
bool L0HwHelperHw<Family>::usmCompressionSupported(const NEO::HardwareInfo &hwInfo) const {
    if (NEO::DebugManager.flags.RenderCompressedBuffersEnabled.get() != -1) {
        return !!NEO::DebugManager.flags.RenderCompressedBuffersEnabled.get();
    }

    return false;
}

template <typename Family>
bool L0HwHelperHw<Family>::forceDefaultUsmCompressionSupport() const {
    return false;
}

template <typename Family>
bool L0HwHelperHw<Family>::alwaysAllocateEventInLocalMem() const {
    return false;
}

template <typename Family>
bool L0HwHelperHw<Family>::multiTileCapablePlatform() const {
    return false;
}

} // namespace L0
