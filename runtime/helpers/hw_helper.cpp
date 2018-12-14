/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/hw_helper.h"
#include "runtime/os_interface/debug_settings_manager.h"

namespace OCLRT {
HwHelper *hwHelperFactory[IGFX_MAX_CORE] = {};

HwHelper &HwHelper::get(GFXCORE_FAMILY gfxCore) {
    return *hwHelperFactory[gfxCore];
}

bool HwHelper::renderCompressedBuffersSupported(const HardwareInfo &hwInfo) {
    if (DebugManager.flags.RenderCompressedBuffersEnabled.get() != -1) {
        return !!DebugManager.flags.RenderCompressedBuffersEnabled.get();
    }
    return hwInfo.capabilityTable.ftrRenderCompressedBuffers;
}

bool HwHelper::renderCompressedImagesSupported(const HardwareInfo &hwInfo) {
    if (DebugManager.flags.RenderCompressedImagesEnabled.get() != -1) {
        return !!DebugManager.flags.RenderCompressedImagesEnabled.get();
    }
    return hwInfo.capabilityTable.ftrRenderCompressedImages;
}

} // namespace OCLRT
