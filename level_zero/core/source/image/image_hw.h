/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/hw_info.h"

#include "level_zero/core/source/image/image_imp.h"

namespace L0 {
struct StructuresLookupTable;

template <GFXCORE_FAMILY gfxCoreFamily>
struct ImageCoreFamily : public ImageImp {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;
    using ImageImp::bindlessImage;

    ze_result_t initialize(Device *device, const ze_image_desc_t *desc) override;
    void copySurfaceStateToSSH(void *surfaceStateHeap,
                               uint32_t surfaceStateOffset,
                               uint32_t bindlessSlot,
                               bool isMediaBlockArg) override;
    bool isMediaFormat(const ze_image_format_layout_t layout) {
        if (layout == ze_image_format_layout_t::ZE_IMAGE_FORMAT_LAYOUT_NV12 ||
            layout == ze_image_format_layout_t::ZE_IMAGE_FORMAT_LAYOUT_P010 ||
            layout == ze_image_format_layout_t::ZE_IMAGE_FORMAT_LAYOUT_P012 ||
            layout == ze_image_format_layout_t::ZE_IMAGE_FORMAT_LAYOUT_P016 ||
            layout == ze_image_format_layout_t::ZE_IMAGE_FORMAT_LAYOUT_RGBP ||
            layout == ze_image_format_layout_t::ZE_IMAGE_FORMAT_LAYOUT_BRGP) {
            return true;
        }
        return false;
    }

    static constexpr uint32_t zeImageFormatSwizzleMax = ZE_IMAGE_FORMAT_SWIZZLE_X + 1u;

    const std::array<typename RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT, zeImageFormatSwizzleMax> shaderChannelSelect = {
        RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED,
        RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN,
        RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE,
        RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA,
        RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO,
        RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ONE,
        RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO};

  protected:
    bool isSuitableForCompression(const StructuresLookupTable &structuresLookupTable, const NEO::ImageInfo &imgInfo);

    RENDER_SURFACE_STATE surfaceState;
    RENDER_SURFACE_STATE implicitArgsSurfaceState;
    RENDER_SURFACE_STATE redescribedSurfaceState;
    RENDER_SURFACE_STATE packedSurfaceState;
};

template <uint32_t gfxProductFamily>
struct ImageProductFamily;

} // namespace L0
