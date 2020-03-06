/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/indirect_heap/indirect_heap.h"

#include "level_zero/core/source/image_formats_append.h"
#include "level_zero/core/source/image_imp.h"

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
struct ImageCoreFamily : public ImageImp {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;
    using RSS = typename GfxFamily::RENDER_SURFACE_STATE;
    using RENDER_FORMAT = typename RENDER_SURFACE_STATE::SURFACE_FORMAT;
    using SHADER_CHANNEL_SELECT = typename RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT;
    using BaseClass = ImageImp;
    using ImageImp::ImageImp;

    static const RENDER_FORMAT surfaceFormatUndefined = static_cast<RENDER_FORMAT>(-1);

    static const GMM_RESOURCE_FORMAT gmmResourceFormatUndefined = static_cast<GMM_RESOURCE_FORMAT>(-1);

    NEO::SurfaceFormatInfo surfaceFormatsForRedescribe[5] = {
        {GMM_FORMAT_R8_UINT_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R8_UINT), 0, 1, 1, 1},
        {GMM_FORMAT_R16_UINT_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R16_UINT), 0, 1, 2, 2},
        {GMM_FORMAT_R32_UINT_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R32_UINT), 0, 1, 4, 4},
        {GMM_FORMAT_R32G32_UINT_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R32G32_UINT), 0, 2, 4, 8},
        {GMM_FORMAT_R32G32B32A32_UINT_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R32G32B32A32_UINT), 0, 4, 4, 16}};

    NEO::SurfaceFormatInfo surfaceFormatTable[ZE_IMAGE_FORMAT_RENDER_LAYOUT_MAX + 1]
                                             [ZE_IMAGE_FORMAT_TYPE_MAX + 1] = {
                                                 // ZE_IMAGE_FORMAT_LAYOUT_8
                                                 {{GMM_FORMAT_R8_UINT_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R8_UINT), 0, 1, 1, 1}, {GMM_FORMAT_R8_SINT_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R8_SINT), 0, 1, 1, 1}, {GMM_FORMAT_R8_UNORM_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R8_UNORM), 0, 1, 1, 1}, {GMM_FORMAT_R8_SNORM_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R8_SNORM), 0, 1, 1, 1}, {gmmResourceFormatUndefined, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(surfaceFormatUndefined), 0, 1, 1, 1}},
                                                 // ZE_IMAGE_FORMAT_LAYOUT_16
                                                 {{GMM_FORMAT_R16_UINT_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R16_UINT), 0, 1, 2, 2}, {GMM_FORMAT_R16_SINT_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R16_SINT), 0, 1, 2, 2}, {GMM_FORMAT_R16_UNORM_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R16_UNORM), 0, 1, 2, 2}, {GMM_FORMAT_R16_SNORM_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R16_SNORM), 0, 1, 2, 2}, {GMM_FORMAT_R16_FLOAT_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R16_FLOAT), 0, 1, 2, 2}},
                                                 // ZE_IMAGE_FORMAT_LAYOUT_32
                                                 {{GMM_FORMAT_R32_UINT_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R32_UINT), 0, 1, 4, 4}, {GMM_FORMAT_R32_SINT_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R32_SINT), 0, 1, 4, 4}, {GMM_FORMAT_R32_UNORM_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R32_UNORM), 0, 1, 4, 4}, {GMM_FORMAT_R32_SNORM_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R32_SNORM), 0, 1, 4, 4}, {GMM_FORMAT_R32_FLOAT_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R32_FLOAT), 0, 1, 4, 4}},
                                                 // ZE_IMAGE_FORMAT_LAYOUT_8_8
                                                 {{GMM_FORMAT_R8G8_UINT_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R8G8_UINT), 0, 2, 1, 2}, {GMM_FORMAT_R8G8_SINT_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R8G8_SINT), 0, 2, 1, 2}, {GMM_FORMAT_R8G8_UNORM_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R8G8_UNORM), 0, 2, 1, 2}, {GMM_FORMAT_R8G8_SNORM_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R8G8_SNORM), 0, 2, 1, 2}, {gmmResourceFormatUndefined, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(surfaceFormatUndefined), 0, 2, 1, 2}},
                                                 // ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8
                                                 {{GMM_FORMAT_R8G8B8A8_UINT_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R8G8B8A8_UINT), 0, 4, 1, 4}, {GMM_FORMAT_R8G8B8A8_SINT_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R8G8B8A8_SINT), 0, 4, 1, 4}, {GMM_FORMAT_R8G8B8A8_UNORM_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R8G8B8A8_UNORM), 0, 4, 1, 4}, {GMM_FORMAT_R8G8B8A8_SNORM_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R8G8B8A8_SNORM), 0, 4, 1, 4}, {gmmResourceFormatUndefined, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(surfaceFormatUndefined), 0, 4, 1, 4}},
                                                 // ZE_IMAGE_FORMAT_LAYOUT_16_16
                                                 {{GMM_FORMAT_R16G16_UINT_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R16G16_UINT), 0, 2, 2, 4}, {GMM_FORMAT_R16G16_SINT_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R16G16_SINT), 0, 2, 2, 4}, {GMM_FORMAT_R16G16_UNORM_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R16G16_UNORM), 0, 2, 2, 4}, {GMM_FORMAT_R16G16_SNORM_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R16G16_SNORM), 0, 2, 2, 4}, {GMM_FORMAT_R16G16_FLOAT_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R16G16_FLOAT), 0, 2, 2, 4}},
                                                 // ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16
                                                 {{GMM_FORMAT_R16G16B16A16_UINT_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R16G16B16A16_UINT), 0, 4, 2, 8}, {GMM_FORMAT_R16G16B16A16_SINT_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R16G16B16A16_SINT), 0, 4, 2, 8}, {GMM_FORMAT_R16G16B16A16_UNORM_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R16G16B16A16_UNORM), 0, 4, 2, 8}, {GMM_FORMAT_R16G16B16A16_SNORM_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R16G16B16A16_SNORM), 0, 4, 2, 8}, {GMM_FORMAT_R16G16B16A16_FLOAT_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R16G16B16A16_FLOAT), 0, 4, 2, 8}},
                                                 // ZE_IMAGE_FORMAT_LAYOUT_32_32
                                                 {{GMM_FORMAT_R32G32_UINT_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R32G32_UINT), 0, 2, 4, 8}, {GMM_FORMAT_R32G32_SINT_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R32G32_SINT), 0, 2, 4, 8}, {GMM_FORMAT_R32G32_UNORM_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R32G32_UNORM), 0, 2, 4, 8}, {GMM_FORMAT_R32G32_SNORM_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R32G32_SNORM), 0, 2, 4, 8}, {GMM_FORMAT_R32G32_FLOAT_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R32G32_FLOAT), 0, 2, 4, 8}},
                                                 // ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32
                                                 {{GMM_FORMAT_R32G32B32A32_UINT_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R32G32B32A32_UINT), 0, 4, 4, 16}, {GMM_FORMAT_R32G32B32A32_SINT_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R32G32B32A32_SINT), 0, 4, 4, 16}, {GMM_FORMAT_R32G32B32A32_UNORM_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R32G32B32A32_UNORM), 0, 4, 4, 16}, {GMM_FORMAT_R32G32B32A32_SNORM_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R32G32B32A32_SNORM), 0, 4, 4, 16}, {GMM_FORMAT_R32G32B32A32_FLOAT_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R32G32B32A32_FLOAT), 0, 4, 4, 16}},
                                                 // ZE_IMAGE_FORMAT_LAYOUT_10_10_10_2
                                                 {{GMM_FORMAT_R10G10B10A2_UINT_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R10G10B10A2_UINT), 0, 1, 1, 1}, {GMM_FORMAT_R10G10B10A2_SINT_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R10G10B10A2_SINT), 0, 1, 1, 1}, {GMM_FORMAT_R10G10B10A2_UNORM_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R10G10B10A2_UNORM), 0, 1, 1, 1}, {GMM_FORMAT_R10G10B10A2_SNORM_TYPE, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(RSS::SURFACE_FORMAT_R10G10B10A2_SNORM), 0, 1, 1, 1}, {gmmResourceFormatUndefined, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(surfaceFormatUndefined), 0, 1, 1, 1}},
                                                 // ZE_IMAGE_FORMAT_LAYOUT_11_11_10
                                                 {{gmmResourceFormatUndefined, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(surfaceFormatUndefined), 0, 0, 0, 0}, {gmmResourceFormatUndefined, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(surfaceFormatUndefined), 0, 0, 0, 0}, {gmmResourceFormatUndefined, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(surfaceFormatUndefined), 0, 0, 0, 0}, {gmmResourceFormatUndefined, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(surfaceFormatUndefined), 0, 0, 0, 0}, {gmmResourceFormatUndefined, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(surfaceFormatUndefined), 0, 0, 0, 0}},
                                                 // ZE_IMAGE_FORMAT_LAYOUT_5_6_5
                                                 {{gmmResourceFormatUndefined, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(surfaceFormatUndefined), 0, 0, 0, 0}, {gmmResourceFormatUndefined, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(surfaceFormatUndefined), 0, 0, 0, 0}, {gmmResourceFormatUndefined, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(surfaceFormatUndefined), 0, 0, 0, 0}, {gmmResourceFormatUndefined, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(surfaceFormatUndefined), 0, 0, 0, 0}, {gmmResourceFormatUndefined, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(surfaceFormatUndefined), 0, 0, 0, 0}},
                                                 // ZE_IMAGE_FORMAT_LAYOUT_5_5_5_1
                                                 {{gmmResourceFormatUndefined, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(surfaceFormatUndefined), 0, 0, 0, 0}, {gmmResourceFormatUndefined, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(surfaceFormatUndefined), 0, 0, 0, 0}, {gmmResourceFormatUndefined, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(surfaceFormatUndefined), 0, 0, 0, 0}, {gmmResourceFormatUndefined, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(surfaceFormatUndefined), 0, 0, 0, 0}, {gmmResourceFormatUndefined, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(surfaceFormatUndefined), 0, 0, 0, 0}},
                                                 // ZE_IMAGE_FORMAT_LAYOUT_4_4_4_4
                                                 {{gmmResourceFormatUndefined, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(surfaceFormatUndefined), 0, 0, 0, 0}, {gmmResourceFormatUndefined, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(surfaceFormatUndefined), 0, 0, 0, 0}, {gmmResourceFormatUndefined, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(surfaceFormatUndefined), 0, 0, 0, 0}, {gmmResourceFormatUndefined, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(surfaceFormatUndefined), 0, 0, 0, 0}, {gmmResourceFormatUndefined, static_cast<NEO::GFX3DSTATE_SURFACEFORMAT>(surfaceFormatUndefined), 0, 0, 0, 0}}};

    const SHADER_CHANNEL_SELECT shaderChannelSelect[ZE_IMAGE_FORMAT_SWIZZLE_MAX + 1] = {
        RSS::SHADER_CHANNEL_SELECT_RED,
        RSS::SHADER_CHANNEL_SELECT_GREEN,
        RSS::SHADER_CHANNEL_SELECT_BLUE,
        RSS::SHADER_CHANNEL_SELECT_ALPHA,
        RSS::SHADER_CHANNEL_SELECT_ZERO,
        RSS::SHADER_CHANNEL_SELECT_ONE,
        RSS::SHADER_CHANNEL_SELECT_ZERO,
    };

    bool initialize(Device *device, const ze_image_desc_t *desc) override;

    void copySurfaceStateToSSH(void *surfaceStateHeap, const uint32_t surfaceStateOffset) override;
    void copyRedescribedSurfaceStateToSSH(void *surfaceStateHeap, const uint32_t surfaceStateOffset) override;

  protected:
    RENDER_SURFACE_STATE surfaceState;
    RENDER_SURFACE_STATE redescribedSurfaceState;
};

template <uint32_t gfxProductFamily>
struct ImageProductFamily;

} // namespace L0
