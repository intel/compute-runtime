/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/helpers/surface_format_info.h"

#include "level_zero/core/source/image/image.h"

#include <memory>
#include <mutex>
#include <optional>

namespace NEO {
class GraphicsAllocation;
struct SurfaceStateInHeapInfo;
} // namespace NEO

namespace L0 {
struct Device;

struct ImageImp : public Image, NEO::NonCopyableAndNonMovableClass {
    ze_result_t destroy() override;
    ze_result_t destroyPeerImages(const void *ptr, Device *device) override;

    virtual ze_result_t initialize(Device *device, const ze_image_desc_t *desc) = 0;

    ~ImageImp() override;

    NEO::GraphicsAllocation *getAllocation() override { return allocation; }
    NEO::GraphicsAllocation *getImplicitArgsAllocation() override { return implicitArgsAllocation; }
    NEO::ImageInfo getImageInfo() override { return imgInfo; }
    ze_image_desc_t getImageDesc() override {
        return imageFormatDesc;
    }

    ze_result_t createView(Device *device, const ze_image_desc_t *desc, ze_image_handle_t *pImage) override;

    ze_result_t getMemoryProperties(ze_image_memory_properties_exp_t *pMemoryProperties) override {
        pMemoryProperties->rowPitch = imgInfo.rowPitch;
        pMemoryProperties->slicePitch = imgInfo.slicePitch;
        pMemoryProperties->size = imgInfo.size;

        return ZE_RESULT_SUCCESS;
    }

    bool isImageView() const {
        return sourceImageFormatDesc.has_value();
    }

    bool isMimickedImage() override {
        return mimickedImagefor3Ch;
    }

    bool isSrgb() const override { return srgbImage; }

    void setMimickedImage(bool value) {
        this->mimickedImagefor3Ch = value;
    }

    void populateImageImplicitArgs(NEO::ImageImplicitArgs &imageImplicitArgs);
    ze_result_t allocateBindlessSlot() override;
    NEO::SurfaceStateInHeapInfo *getBindlessSlot() override;
    ze_result_t getDeviceOffset(uint64_t *deviceOffset) override;
    static size_t getRowPitchFor2dImage(Device *device, const NEO::ImageInfo &imgInfo);
    ze_result_t allocateImplicitArgsOnDemand() override;
    void encodeImplicitArgsSurfaceState() override {};

    void applyGlTextureExtOverrides(NEO::GraphicsAllocation *allocation,
                                    NEO::ImageInfo &imgInfo,
                                    void *pGmmResInfo,
                                    uint64_t textureBufferOffset,
                                    uint32_t glHWFormat,
                                    bool isAuxEnabled,
                                    uint32_t numberOfSamples,
                                    void *mcsNtHandle,
                                    void *pGmmResInfoMcs,
                                    bool hasUnifiedMcsSurface);

    NEO::GraphicsAllocation *getMcsAllocation() override { return mcsAllocation; }
    uint32_t getMcsPitch() const { return mcsPitch; }
    uint32_t getMcsQPitch() const { return mcsQPitch; }
    uint32_t getMcsMultisampleCount() const { return mcsMultisampleCount; }
    uint32_t getNumSamples() const { return numSamples; }
    bool getIsUnifiedMcsSurface() const { return isUnifiedMcsSurface; }

  protected:
    Device *device = nullptr;
    NEO::ImageInfo imgInfo = {};
    NEO::GraphicsAllocation *allocation = nullptr;
    NEO::GraphicsAllocation *implicitArgsAllocation = nullptr;
    NEO::GraphicsAllocation *mcsAllocation = nullptr;
    uint32_t mcsPitch = 0;
    uint32_t mcsQPitch = 0;
    uint32_t mcsMultisampleCount = 0;
    uint32_t numSamples = 0;
    bool isUnifiedMcsSurface = false;
    std::mutex implicitArgsAllocationMutex;
    ze_image_desc_t imageFormatDesc = {};
    ze_sampler_desc_t samplerDesc = {};
    std::optional<ze_image_desc_t> sourceImageFormatDesc = {};
    std::unique_ptr<NEO::SurfaceStateInHeapInfo> bindlessInfo;
    bool bindlessImage = false;
    bool imageFromBuffer = false;
    bool sampledImage = false;
    bool mimickedImagefor3Ch = false;
    bool srgbImage = false;
    NEO::SurfaceFormatInfo overriddenSurfaceFormat = {};
    bool hasOverriddenSurfaceFormat = false;
};

static_assert(NEO::NonCopyableAndNonMovable<ImageImp>);

} // namespace L0
