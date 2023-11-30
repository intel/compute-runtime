/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/xe_hpg_core/hw_cmds.h"

#include "encode_surface_state_args.h"

namespace NEO {

template <>
void EncodeSurfaceState<Family>::encodeExtraCacheSettings(R_SURFACE_STATE *surfaceState, const EncodeSurfaceStateArgs &args) {
    using L1_CACHE_POLICY = typename R_SURFACE_STATE::L1_CACHE_POLICY;
    auto &productHelper = args.gmmHelper->getRootDeviceEnvironment().getHelper<ProductHelper>();

    auto cachePolicy = static_cast<L1_CACHE_POLICY>(productHelper.getL1CachePolicy(args.isDebuggerActive));
    if (debugManager.flags.OverrideL1CacheControlInSurfaceState.get() != -1 &&
        debugManager.flags.ForceAllResourcesUncached.get() == false) {
        cachePolicy = static_cast<L1_CACHE_POLICY>(debugManager.flags.OverrideL1CacheControlInSurfaceState.get());
    }
    surfaceState->setL1CachePolicyL1CacheControl(cachePolicy);
}

template <typename GfxFamily>
void EncodeEnableRayTracing<GfxFamily>::programEnableRayTracing(LinearStream &commandStream, uint64_t backBuffer) {
    auto cmd = GfxFamily::cmd3dStateBtd;
    cmd.getBtdStateBody().setPerDssMemoryBackedBufferSize(static_cast<typename GfxFamily::_3DSTATE_BTD_BODY::PER_DSS_MEMORY_BACKED_BUFFER_SIZE>(RayTracingHelper::getMemoryBackedFifoSizeToPatch()));
    cmd.getBtdStateBody().setMemoryBackedBufferBasePointer(backBuffer);
    append3dStateBtd(&cmd);
    *commandStream.getSpaceForCmd<typename GfxFamily::_3DSTATE_BTD>() = cmd;
}

template <>
inline void EncodeWA<Family>::setAdditionalPipeControlFlagsForNonPipelineStateCommand(PipeControlArgs &args) {
    args.unTypedDataPortCacheFlush = true;
}

} // namespace NEO
