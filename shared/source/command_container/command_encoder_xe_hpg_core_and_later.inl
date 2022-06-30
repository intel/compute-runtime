/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/xe_hpg_core/hw_cmds.h"

namespace NEO {

template <>
void EncodeSurfaceState<Family>::encodeExtraCacheSettings(R_SURFACE_STATE *surfaceState, const HardwareInfo &hwInfo) {
    using L1_CACHE_POLICY = typename R_SURFACE_STATE::L1_CACHE_POLICY;
    auto hwInfoConfig = HwInfoConfig::get(hwInfo.platform.eProductFamily);
    auto cachePolicy = static_cast<L1_CACHE_POLICY>(hwInfoConfig->getL1CachePolicy());
    surfaceState->setL1CachePolicyL1CacheControl(cachePolicy);

    if (DebugManager.flags.OverrideL1CacheControlInSurfaceState.get() != -1) {
        surfaceState->setL1CachePolicyL1CacheControl(static_cast<L1_CACHE_POLICY>(DebugManager.flags.OverrideL1CacheControlInSurfaceState.get()));
    }
}

template <typename GfxFamily>
void EncodeEnableRayTracing<GfxFamily>::programEnableRayTracing(LinearStream &commandStream, GraphicsAllocation &backBuffer) {
    auto cmd = GfxFamily::cmd3dStateBtd;
    cmd.getBtdStateBody().setPerDssMemoryBackedBufferSize(static_cast<typename GfxFamily::_3DSTATE_BTD_BODY::PER_DSS_MEMORY_BACKED_BUFFER_SIZE>(RayTracingHelper::getMemoryBackedFifoSizeToPatch()));
    cmd.getBtdStateBody().setMemoryBackedBufferBasePointer(backBuffer.getGpuAddress());
    append3dStateBtd(&cmd);
    *commandStream.getSpaceForCmd<typename GfxFamily::_3DSTATE_BTD>() = cmd;
}

template <>
inline void EncodeWA<Family>::setAdditionalPipeControlFlagsForNonPipelineStateCommand(PipeControlArgs &args) {
    args.unTypedDataPortCacheFlush = true;
}

} // namespace NEO
