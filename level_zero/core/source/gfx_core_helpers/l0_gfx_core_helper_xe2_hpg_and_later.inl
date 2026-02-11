/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"

namespace L0 {

template <typename Family>
bool L0GfxCoreHelperHw<Family>::imageCompressionSupported(const NEO::HardwareInfo &hwInfo) const {
    if (NEO::debugManager.flags.RenderCompressedImagesEnabled.get() != -1) {
        return !!NEO::debugManager.flags.RenderCompressedImagesEnabled.get();
    }

    return hwInfo.capabilityTable.ftrRenderCompressedImages;
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::usmCompressionSupported(const NEO::HardwareInfo &hwInfo) const {
    if (NEO::debugManager.flags.RenderCompressedBuffersEnabled.get() != -1) {
        return !!NEO::debugManager.flags.RenderCompressedBuffersEnabled.get();
    }

    return hwInfo.capabilityTable.ftrRenderCompressedBuffers;
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::forceDefaultUsmCompressionSupport() const {
    return true;
}

template <typename Family>
ze_record_replay_graph_exp_flags_t L0GfxCoreHelperHw<Family>::getPlatformRecordReplayGraphCapabilities() const {
    return ZE_RECORD_REPLAY_GRAPH_EXP_FLAG_IMMUTABLE_GRAPH;
}

template <typename Family>
uint64_t L0GfxCoreHelperHw<Family>::getOaTimestampValidBits() const {
    constexpr uint64_t oaTimestampValidBits = 56u;
    return oaTimestampValidBits;
};

} // namespace L0
