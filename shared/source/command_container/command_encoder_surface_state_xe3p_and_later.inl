/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/surface_format_info.h"

namespace NEO {

template <typename Family>
void EncodeSurfaceState<Family>::convertSurfaceStateToPacked(R_SURFACE_STATE *surfaceState, ImageInfo &imgInfo) {
    using SURFACE_FORMAT = typename R_SURFACE_STATE::SURFACE_FORMAT;
    using SHADER_CHANNEL_SELECT = typename R_SURFACE_STATE::SHADER_CHANNEL_SELECT;

    constexpr uint32_t r32g32b32a32SurfaceFormatImageElementSizeInBytes = 16u;

    constexpr auto convertSurfaceFormatToPackedLambda = [](uint32_t bitsPerPixel) -> typename R_SURFACE_STATE::OVERRIDE_TILE_BPP {
        using OVERRIDE_TILE_BPP = typename R_SURFACE_STATE::OVERRIDE_TILE_BPP;

        switch (bitsPerPixel) {
        case 64:
            return OVERRIDE_TILE_BPP::OVERRIDE_TILE_BPP_TILE64_64BPP;
        case 32:
            return OVERRIDE_TILE_BPP::OVERRIDE_TILE_BPP_TILE64_32BPP;
        case 16:
            return OVERRIDE_TILE_BPP::OVERRIDE_TILE_BPP_TILE64_16BPP;
        case 8:
            return OVERRIDE_TILE_BPP::OVERRIDE_TILE_BPP_TILE64_8BPP;
        default:
            return OVERRIDE_TILE_BPP::OVERRIDE_TILE_BPP_TILE64_LEGACY;
        }
    };

    auto scale = r32g32b32a32SurfaceFormatImageElementSizeInBytes / imgInfo.surfaceFormat->imageElementSizeInBytes;
    auto newWidth = static_cast<uint32_t>(Math::divideAndRoundUp(imgInfo.imgDesc.imageWidth, scale));

    surfaceState->setOverrideTileBPP(convertSurfaceFormatToPackedLambda(static_cast<uint32_t>(imgInfo.surfaceFormat->imageElementSizeInBytes * 8)));
    surfaceState->setWidth(newWidth);
    surfaceState->setSurfaceFormat(SURFACE_FORMAT::SURFACE_FORMAT_R32G32B32A32_UINT);

    surfaceState->setShaderChannelSelectRed(SHADER_CHANNEL_SELECT::SHADER_CHANNEL_SELECT_RED);
    surfaceState->setShaderChannelSelectGreen(SHADER_CHANNEL_SELECT::SHADER_CHANNEL_SELECT_GREEN);
    surfaceState->setShaderChannelSelectBlue(SHADER_CHANNEL_SELECT::SHADER_CHANNEL_SELECT_BLUE);
    surfaceState->setShaderChannelSelectAlpha(SHADER_CHANNEL_SELECT::SHADER_CHANNEL_SELECT_ALPHA);
}

template <typename Family>
void EncodeSurfaceState<Family>::setAdditionalCacheSettings(R_SURFACE_STATE *surfaceState) {
    int32_t forceL1P5CacheForRenderSurface = debugManager.flags.ForceL1P5CacheForRenderSurface.get();
    if (forceL1P5CacheForRenderSurface != -1) {
        surfaceState->setDisableL1P5(!forceL1P5CacheForRenderSurface);
    }
}

} // namespace NEO
