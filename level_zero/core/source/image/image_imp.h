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

#include <initializer_list>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace NEO {
class GraphicsAllocation;
class TagNodeBase;
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
    const NEO::ImageInfo &getImageInfo() override { return imgInfo; }
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

    bool hasCustomPitch() const override { return customRowPitch > 0 || customSlicePitch > 0; }
    size_t getCustomRowPitch() const override { return customRowPitch; }
    size_t getCustomSlicePitch() const override { return customSlicePitch; }

    bool isSrgb() const override { return srgbImage; }
    bool isDepthStencil() const override { return depthStencilImage; }

    void setMimickedImage(bool value) {
        this->mimickedImagefor3Ch = value;
    }

    void populateImageImplicitArgs(NEO::ImageImplicitArgs &imageImplicitArgs);
    bool obtainImplicitArgsTag();
    bool writeImplicitArgsToTag();
    static cl_channel_type overrideChannelTypeForDepthInt24Image(cl_channel_type clChannelType, bool isDepthStencil,
                                                                 NEO::GraphicsAllocation *allocation);
    ze_result_t allocateBindlessSlot() override;
    NEO::SurfaceStateInHeapInfo *getBindlessSlot() override;
    ze_result_t allocateBindlessSlotWithMipmap(uint32_t mipLevel) override;
    NEO::SurfaceStateInHeapInfo *getBindlessSlotWithMipmap(uint32_t mipLevel) override;
    NEO::SurfaceStateInHeapInfo *getOrInitializeBindlessSlot(uint32_t bindlessSlot, uint32_t mipLevel, bool isMediaBlockArg) override;
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
    NEO::SurfaceStateInHeapInfo *allocateBindlessSlotCopy(std::unique_ptr<NEO::SurfaceStateInHeapInfo> &slotCopy,
                                                          std::initializer_list<uint32_t> bindlessSlots,
                                                          uint32_t mipLevel,
                                                          bool isMediaBlockArg);

    Device *device = nullptr;
    NEO::ImageInfo imgInfo = {};
    NEO::GraphicsAllocation *allocation = nullptr;
    NEO::GraphicsAllocation *implicitArgsAllocation = nullptr;
    NEO::TagNodeBase *implicitArgsTag = nullptr;
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
    std::vector<std::unique_ptr<NEO::SurfaceStateInHeapInfo>> mipLevelBindlessInfos;
    std::unique_ptr<NEO::SurfaceStateInHeapInfo> mediaBlockBindlessInfo;
    uint32_t bindlessSlotsWrittenMask = 0u;
    std::mutex mipLevelBindlessInfosMutex;
    bool bindlessImage = false;
    bool imageFromBuffer = false;
    bool sampledImage = false;
    bool mimickedImagefor3Ch = false;
    bool srgbImage = false;
    size_t customRowPitch = 0;
    size_t customSlicePitch = 0;
    bool depthStencilImage = false;
    NEO::SurfaceFormatInfo overriddenSurfaceFormat = {};
    bool hasOverriddenSurfaceFormat = false;
};

static_assert(NEO::NonCopyableAndNonMovable<ImageImp>);

} // namespace L0
