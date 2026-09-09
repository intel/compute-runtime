/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/hw_mapper.h"

#include "level_zero/core/source/image/image_imp.h"

#include <array>
#include <cstdint>

namespace NEO {
class Gmm;
class GmmHelper;
} // namespace NEO

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
                               bool isMediaBlockArg,
                               uint32_t mipLevel) override;
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
    bool isPackedYuvFormat(const ze_image_format_layout_t layout) {
        if (layout == ze_image_format_layout_t::ZE_IMAGE_FORMAT_LAYOUT_YUYV ||
            layout == ze_image_format_layout_t::ZE_IMAGE_FORMAT_LAYOUT_VYUY ||
            layout == ze_image_format_layout_t::ZE_IMAGE_FORMAT_LAYOUT_YVYU ||
            layout == ze_image_format_layout_t::ZE_IMAGE_FORMAT_LAYOUT_UYVY) {
            return true;
        }
        return false;
    }
    bool hasIgnoredFormatType(const ze_image_format_layout_t layout) {
        return layout >= ze_image_format_layout_t::ZE_IMAGE_FORMAT_LAYOUT_Y8 &&
               layout <= ze_image_format_layout_t::ZE_IMAGE_FORMAT_LAYOUT_BRGP;
    }
    bool hasValidSwizzles(const ze_image_format_t &format) {
        return (static_cast<uint32_t>(format.x) < zeImageFormatSwizzleMax) &&
               (static_cast<uint32_t>(format.y) < zeImageFormatSwizzleMax) &&
               (static_cast<uint32_t>(format.z) < zeImageFormatSwizzleMax) &&
               (static_cast<uint32_t>(format.w) < zeImageFormatSwizzleMax);
    }
    void encodeImplicitArgsSurfaceState() override;
    static constexpr uint32_t zeImageFormatSwizzleMax = ZE_IMAGE_FORMAT_SWIZZLE_D + 1u;

    const std::array<typename RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT, zeImageFormatSwizzleMax> shaderChannelSelect = {
        RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED,
        RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN,
        RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE,
        RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA,
        RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO,
        RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ONE,
        RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO,
        RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED};

  protected:
    bool isSuitableForCompression(const StructuresLookupTable &structuresLookupTable, const NEO::ImageInfo &imgInfo);

    struct SurfaceStateSlotContext {
        const ze_image_desc_t *desc = nullptr;
        const NEO::ImageInfo *redescribedImageInfo = nullptr;
        const NEO::SurfaceOffsets *surfaceOffsets = nullptr;
        NEO::Gmm *gmm = nullptr;
        NEO::GmmHelper *gmmHelper = nullptr;
        typename RENDER_SURFACE_STATE::SURFACE_TYPE surfaceType = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_2D;
        uint32_t cubeFaceIndex = 0u;
        uint32_t numSamples = 1u;
        bool isMediaFormatLayout = false;
        bool hasFixedChannelSelect = false;
        bool redescribedIsNV12 = false;
        bool packedSupported = false;
    };

    void encodeSurfaceState(const SurfaceStateSlotContext &context);

  private:
    void encodeSurfaceStateReduced(const SurfaceStateSlotContext &context);
    void encodeSurfaceStateFull(const SurfaceStateSlotContext &context);

  protected:
    using SurfaceStateSlotStorage = std::array<uint8_t, sizeof(RENDER_SURFACE_STATE)>;
    static_assert(alignof(RENDER_SURFACE_STATE) == 1u);

    RENDER_SURFACE_STATE &getSurfaceState() {
        return *reinterpret_cast<RENDER_SURFACE_STATE *>(surfaceStateStorage.data());
    }
    RENDER_SURFACE_STATE &getRedescribedSurfaceState() {
        return *reinterpret_cast<RENDER_SURFACE_STATE *>(redescribedSurfaceStateStorage.data());
    }
    RENDER_SURFACE_STATE &getPackedSurfaceState() {
        return *reinterpret_cast<RENDER_SURFACE_STATE *>(packedSurfaceStateStorage.data());
    }
    RENDER_SURFACE_STATE &getImplicitArgsSurfaceState() {
        return *reinterpret_cast<RENDER_SURFACE_STATE *>(implicitArgsSurfaceStateStorage.data());
    }

    SurfaceStateSlotStorage surfaceStateStorage = {};
    SurfaceStateSlotStorage implicitArgsSurfaceStateStorage = {};
    SurfaceStateSlotStorage redescribedSurfaceStateStorage = {};
    SurfaceStateSlotStorage packedSurfaceStateStorage = {};
};

template <uint32_t gfxProductFamily>
struct ImageProductFamily;

} // namespace L0
