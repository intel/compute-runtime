/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/supported_media_surface_formats.h"

#include "shared/source/helpers/surface_format_info.h"

#include <algorithm>
#include <array>

namespace NEO {
constexpr std::array<NEO::GFX3DSTATE_SURFACEFORMAT, 42> supportedMediaSurfaceFormats = {NEO::GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_UNORM,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_B8G8R8A8_UNORM,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_R10G10B10A2_UNORM,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_R10G10B10A2_UINT,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_UNORM,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_SNORM,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_SINT,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_UINT,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_R16G16_UNORM,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_R16G16_SNORM,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_R16G16_SINT,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_R16G16_UINT,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_R16G16_FLOAT,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_B10G10R10A2_UNORM,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_R11G11B10_FLOAT,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_R32_SINT,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_R32_UINT,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_R32_FLOAT,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_B5G6R5_UNORM,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_B5G5R5A1_UNORM,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_B4G4R4A4_UNORM,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_R8G8_UNORM,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_R8G8_SNORM,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_R8G8_SINT,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_R8G8_UINT,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_R16_UNORM,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_R16_SNORM,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_R16_SINT,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_R16_UINT,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_R16_FLOAT,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_A16_UNORM,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_B5G5R5X1_UNORM,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_R8_UNORM,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_R8_SNORM,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_R8_SINT,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_R8_UINT,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_A8_UNORM,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_YCRCB_NORMAL,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_YCRCB_SWAPUVY,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_YCRCB_SWAPUV,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_YCRCB_SWAPY,
                                                                                        NEO::GFX3DSTATE_SURFACEFORMAT_PACKED_422_16};
bool SupportedMediaFormatsHelper::isMediaFormatSupported(NEO::GFX3DSTATE_SURFACEFORMAT format) {
    return std::find(NEO::supportedMediaSurfaceFormats.begin(), NEO::supportedMediaSurfaceFormats.end(), format) != NEO::supportedMediaSurfaceFormats.end();
}
} // namespace NEO