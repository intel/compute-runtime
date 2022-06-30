/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"

#include "shared/source/gmm_helper/cache_settings_helper.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/memory_manager/allocation_type.h"

namespace NEO {
Gmm::Gmm(GmmHelper *gmmHelper, const void *alignedPtr, size_t alignedSize, size_t alignment, GMM_RESOURCE_USAGE_TYPE_ENUM gmmResourceUsage,
         bool preferCompressed, StorageInfo storageInfo, bool allowLargePages) : gmmHelper(gmmHelper) {
    resourceParams.Type = RESOURCE_BUFFER;
    resourceParams.Format = GMM_FORMAT_GENERIC_8BIT;
    resourceParams.BaseWidth64 = static_cast<uint64_t>(alignedSize);
    resourceParams.BaseHeight = 1;
    resourceParams.Depth = 1;
    resourceParams.BaseAlignment = static_cast<uint32_t>(alignment);
    if ((nullptr == alignedPtr) && (false == allowLargePages)) {
        resourceParams.Flags.Info.NoOptimizationPadding = true;
        if ((resourceParams.BaseWidth64 & MemoryConstants::page64kMask) == 0) {
            resourceParams.BaseWidth64 += MemoryConstants::pageSize;
        }
    }

    resourceParams.Usage = gmmResourceUsage;
    resourceParams.Flags.Info.Linear = 1;
    resourceParams.Flags.Info.Cacheable = !CacheSettingsHelper::isUncachedType(gmmResourceUsage);
    resourceParams.Flags.Gpu.Texture = 1;

    if (alignedPtr) {
        resourceParams.Flags.Info.ExistingSysMem = 1;
        resourceParams.pExistingSysMem = castToUint64(alignedPtr);
        resourceParams.ExistingSysMemSize = alignedSize;
    } else {
        resourceParams.NoGfxMemory = 1u;
    }

    if (resourceParams.BaseWidth64 >= GmmHelper::maxPossiblePitch) {
        resourceParams.Flags.Gpu.NoRestriction = 1;
    }

    applyAuxFlagsForBuffer(preferCompressed);
    applyMemoryFlags(storageInfo);
    applyAppResource(storageInfo);
    applyDebugOverrides();

    gmmResourceInfo.reset(GmmResourceInfo::create(gmmHelper->getClientContext(), &resourceParams));
}

Gmm::Gmm(GmmHelper *gmmHelper, GMM_RESOURCE_INFO *inputGmm) : Gmm(gmmHelper, inputGmm, false) {}

Gmm::Gmm(GmmHelper *gmmHelper, GMM_RESOURCE_INFO *inputGmm, bool openingHandle) : gmmHelper(gmmHelper) {
    gmmResourceInfo.reset(GmmResourceInfo::create(gmmHelper->getClientContext(), inputGmm, openingHandle));
    applyDebugOverrides();
}

Gmm::~Gmm() = default;

Gmm::Gmm(GmmHelper *gmmHelper, ImageInfo &inputOutputImgInfo, StorageInfo storageInfo, bool preferCompressed) : gmmHelper(gmmHelper) {
    this->resourceParams = {};
    setupImageResourceParams(inputOutputImgInfo, preferCompressed);
    applyMemoryFlags(storageInfo);
    applyAppResource(storageInfo);
    applyDebugOverrides();

    this->gmmResourceInfo.reset(GmmResourceInfo::create(gmmHelper->getClientContext(), &this->resourceParams));
    UNRECOVERABLE_IF(this->gmmResourceInfo == nullptr);

    queryImageParams(inputOutputImgInfo);
}

void Gmm::setupImageResourceParams(ImageInfo &imgInfo, bool preferCompressed) {
    uint64_t imageWidth = static_cast<uint64_t>(imgInfo.imgDesc.imageWidth);
    uint32_t imageHeight = 1;
    uint32_t imageDepth = 1;
    uint32_t imageCount = 1;

    switch (imgInfo.imgDesc.imageType) {
    case ImageType::Image1D:
    case ImageType::Image1DArray:
    case ImageType::Image1DBuffer:
        resourceParams.Type = GMM_RESOURCE_TYPE::RESOURCE_1D;
        break;
    case ImageType::Image2D:
    case ImageType::Image2DArray:
        resourceParams.Type = GMM_RESOURCE_TYPE::RESOURCE_2D;
        imageHeight = static_cast<uint32_t>(imgInfo.imgDesc.imageHeight);
        break;
    case ImageType::Image3D:
        resourceParams.Type = GMM_RESOURCE_TYPE::RESOURCE_3D;
        imageHeight = static_cast<uint32_t>(imgInfo.imgDesc.imageHeight);
        imageDepth = static_cast<uint32_t>(imgInfo.imgDesc.imageDepth);
        break;
    default:
        return;
    }

    if (imgInfo.imgDesc.imageType == ImageType::Image1DArray ||
        imgInfo.imgDesc.imageType == ImageType::Image2DArray) {
        imageCount = static_cast<uint32_t>(imgInfo.imgDesc.imageArraySize);
    }

    resourceParams.Flags.Info.Linear = imgInfo.linearStorage;

    auto &hwHelper = HwHelper::get(gmmHelper->getClientContext()->getHardwareInfo()->platform.eRenderCoreFamily);

    resourceParams.NoGfxMemory = 1; // dont allocate, only query for params

    resourceParams.Usage = CacheSettingsHelper::getGmmUsageType(AllocationType::IMAGE, false, *gmmHelper->getClientContext()->getHardwareInfo());

    resourceParams.Format = imgInfo.surfaceFormat->GMMSurfaceFormat;
    resourceParams.Flags.Gpu.Texture = 1;
    resourceParams.BaseWidth64 = imageWidth;
    resourceParams.BaseHeight = imageHeight;
    resourceParams.Depth = imageDepth;
    resourceParams.ArraySize = imageCount;
    resourceParams.Flags.Wa.__ForceOtherHVALIGN4 = hwHelper.hvAlign4Required();
    resourceParams.MaxLod = imgInfo.baseMipLevel + imgInfo.mipCount;

    applyAuxFlagsForImage(imgInfo, preferCompressed);
}

void Gmm::applyAuxFlagsForBuffer(bool preferCompression) {
    auto hardwareInfo = gmmHelper->getClientContext()->getHardwareInfo();
    bool allowCompression = HwHelper::compressedBuffersSupported(*hardwareInfo) &&
                            preferCompression;

    auto &hwHelper = HwHelper::get(hardwareInfo->platform.eRenderCoreFamily);
    if (allowCompression) {
        hwHelper.applyRenderCompressionFlag(*this, 1);
        resourceParams.Flags.Gpu.CCS = 1;
        resourceParams.Flags.Gpu.UnifiedAuxSurface = 1;
        isCompressionEnabled = true;
    }
    hwHelper.applyAdditionalCompressionSettings(*this, !isCompressionEnabled);
}

void Gmm::applyAuxFlagsForImage(ImageInfo &imgInfo, bool preferCompressed) {
    uint8_t compressionFormat;
    if (this->resourceParams.Flags.Info.MediaCompressed) {
        compressionFormat = gmmHelper->getClientContext()->getMediaSurfaceStateCompressionFormat(imgInfo.surfaceFormat->GMMSurfaceFormat);
    } else {
        compressionFormat = gmmHelper->getClientContext()->getSurfaceStateCompressionFormat(imgInfo.surfaceFormat->GMMSurfaceFormat);
    }

    bool compressionFormatSupported = false;
    if (gmmHelper->getClientContext()->getHardwareInfo()->featureTable.flags.ftrFlatPhysCCS) {
        compressionFormatSupported = compressionFormat != GMM_FLATCCS_FORMAT::GMM_FLATCCS_FORMAT_INVALID;
    } else {
        compressionFormatSupported = compressionFormat != GMM_E2ECOMP_FORMAT::GMM_E2ECOMP_FORMAT_INVALID;
    }

    const bool isPackedYuv = imgInfo.surfaceFormat->GMMSurfaceFormat == GMM_FORMAT_YUY2 ||
                             imgInfo.surfaceFormat->GMMSurfaceFormat == GMM_FORMAT_UYVY ||
                             imgInfo.surfaceFormat->GMMSurfaceFormat == GMM_FORMAT_YVYU ||
                             imgInfo.surfaceFormat->GMMSurfaceFormat == GMM_FORMAT_VYUY;

    auto hwInfo = gmmHelper->getClientContext()->getHardwareInfo();

    bool allowCompression = HwHelper::compressedImagesSupported(*hwInfo) &&
                            preferCompressed &&
                            compressionFormatSupported &&
                            imgInfo.surfaceFormat->GMMSurfaceFormat != GMM_RESOURCE_FORMAT::GMM_FORMAT_NV12 &&
                            imgInfo.plane == GMM_YUV_PLANE_ENUM::GMM_NO_PLANE &&
                            !isPackedYuv;

    auto &hwHelper = HwHelper::get(hwInfo->platform.eRenderCoreFamily);
    if (imgInfo.useLocalMemory || !hwInfo->featureTable.flags.ftrLocalMemory) {
        if (allowCompression) {
            hwHelper.applyRenderCompressionFlag(*this, 1);
            this->resourceParams.Flags.Gpu.CCS = 1;
            this->resourceParams.Flags.Gpu.UnifiedAuxSurface = 1;
            this->resourceParams.Flags.Gpu.IndirectClearColor = 1;
            this->isCompressionEnabled = true;
        }
    }
    hwHelper.applyAdditionalCompressionSettings(*this, !isCompressionEnabled);
}

void Gmm::queryImageParams(ImageInfo &imgInfo) {
    auto imageCount = this->gmmResourceInfo->getArraySize();
    imgInfo.size = this->gmmResourceInfo->getSizeAllocation();

    imgInfo.rowPitch = this->gmmResourceInfo->getRenderPitch();
    if (imgInfo.rowPitch == 0) { // WA
        imgInfo.rowPitch = alignUp(this->gmmResourceInfo->getBaseWidth(), this->gmmResourceInfo->getHAlign());
        imgInfo.rowPitch = imgInfo.rowPitch * (this->gmmResourceInfo->getBitsPerPixel() >> 3);
    }

    // calculate slice pitch
    if ((this->resourceParams.Type == GMM_RESOURCE_TYPE::RESOURCE_2D ||
         this->resourceParams.Type == GMM_RESOURCE_TYPE::RESOURCE_1D) &&
        imageCount == 1) {
        // 2D or 1D or 1Darray with array_size=1
        imgInfo.slicePitch = imgInfo.size;
    } else {
        // 3D Image or 2D-Array or 1D-Arrays (array_size>1)
        GMM_REQ_OFFSET_INFO reqOffsetInfo = {};
        reqOffsetInfo.ReqRender = 1;
        reqOffsetInfo.Slice = 1;
        reqOffsetInfo.ArrayIndex = (imageCount > 1) ? 1 : 0;

        this->gmmResourceInfo->getOffset(reqOffsetInfo);
        imgInfo.slicePitch = reqOffsetInfo.Render.Offset;
        imgInfo.slicePitch += imgInfo.rowPitch * reqOffsetInfo.Render.YOffset;
        imgInfo.slicePitch += reqOffsetInfo.Render.XOffset;
    }

    if (imgInfo.plane != GMM_NO_PLANE) {
        GMM_REQ_OFFSET_INFO reqOffsetInfo = {};
        reqOffsetInfo.ReqRender = 1;
        reqOffsetInfo.Slice = 0;
        reqOffsetInfo.ArrayIndex = 0;
        reqOffsetInfo.Plane = imgInfo.plane;
        this->gmmResourceInfo->getOffset(reqOffsetInfo);
        imgInfo.xOffset = reqOffsetInfo.Render.XOffset / (this->gmmResourceInfo->getBitsPerPixel() / 8);
        imgInfo.yOffset = reqOffsetInfo.Render.YOffset;
        imgInfo.offset = reqOffsetInfo.Render.Offset;
    }

    if (imgInfo.surfaceFormat->GMMSurfaceFormat == GMM_RESOURCE_FORMAT::GMM_FORMAT_NV12 || imgInfo.surfaceFormat->GMMSurfaceFormat == GMM_RESOURCE_FORMAT::GMM_FORMAT_P010) {
        GMM_REQ_OFFSET_INFO reqOffsetInfo = {};
        reqOffsetInfo.ReqLock = 1;
        reqOffsetInfo.Slice = 1;
        reqOffsetInfo.ArrayIndex = 0;
        reqOffsetInfo.Plane = GMM_PLANE_U;
        this->gmmResourceInfo->getOffset(reqOffsetInfo);
        UNRECOVERABLE_IF(reqOffsetInfo.Lock.Pitch == 0);
        imgInfo.yOffsetForUVPlane = reqOffsetInfo.Lock.Offset / reqOffsetInfo.Lock.Pitch;
    }

    imgInfo.qPitch = queryQPitch(this->resourceParams.Type);
}

uint32_t Gmm::queryQPitch(GMM_RESOURCE_TYPE resType) {
    if (gmmHelper->getClientContext()->getHardwareInfo()->platform.eRenderCoreFamily == IGFX_GEN8_CORE && resType == GMM_RESOURCE_TYPE::RESOURCE_3D) {
        return 0;
    }
    return gmmResourceInfo->getQPitch();
}

void Gmm::updateImgInfoAndDesc(ImageInfo &imgInfo, uint32_t arrayIndex) {
    imgInfo.imgDesc.imageWidth = gmmResourceInfo->getBaseWidth();
    imgInfo.imgDesc.imageRowPitch = gmmResourceInfo->getRenderPitch();
    if (imgInfo.imgDesc.imageRowPitch == 0) {
        size_t width = alignUp(imgInfo.imgDesc.imageWidth, gmmResourceInfo->getHAlign());
        imgInfo.imgDesc.imageRowPitch = width * (gmmResourceInfo->getBitsPerPixel() >> 3);
    }
    imgInfo.imgDesc.imageHeight = gmmResourceInfo->getBaseHeight();
    imgInfo.imgDesc.imageDepth = gmmResourceInfo->getBaseDepth();
    imgInfo.imgDesc.imageArraySize = gmmResourceInfo->getArraySize();
    if (imgInfo.imgDesc.imageDepth > 1 || imgInfo.imgDesc.imageArraySize > 1) {
        GMM_REQ_OFFSET_INFO reqOffsetInfo = {};
        reqOffsetInfo.Slice = imgInfo.imgDesc.imageDepth > 1 ? 1 : 0;
        reqOffsetInfo.ArrayIndex = imgInfo.imgDesc.imageArraySize > 1 ? 1 : 0;
        reqOffsetInfo.ReqLock = 1;
        gmmResourceInfo->getOffset(reqOffsetInfo);
        imgInfo.imgDesc.imageSlicePitch = static_cast<size_t>(reqOffsetInfo.Lock.Offset);
    } else {
        imgInfo.imgDesc.imageSlicePitch = gmmResourceInfo->getSizeAllocation();
    }

    updateOffsetsInImgInfo(imgInfo, arrayIndex);
}

void Gmm::updateOffsetsInImgInfo(ImageInfo &imgInfo, uint32_t arrayIndex) {
    GMM_REQ_OFFSET_INFO reqOffsetInfo = {};
    reqOffsetInfo.ReqRender = 1;
    reqOffsetInfo.Slice = 0;
    reqOffsetInfo.ArrayIndex = arrayIndex;
    reqOffsetInfo.Plane = imgInfo.plane;
    gmmResourceInfo->getOffset(reqOffsetInfo);
    UNRECOVERABLE_IF(gmmResourceInfo->getBitsPerPixel() == 0u);
    imgInfo.xOffset = reqOffsetInfo.Render.XOffset / (gmmResourceInfo->getBitsPerPixel() / 8);
    imgInfo.yOffset = reqOffsetInfo.Render.YOffset;
    imgInfo.offset = reqOffsetInfo.Render.Offset;
}

uint8_t Gmm::resourceCopyBlt(void *sys, void *gpu, uint32_t pitch, uint32_t height, unsigned char upload, ImagePlane plane) {
    GMM_RES_COPY_BLT gmmResourceCopyBLT = {};

    if (plane == ImagePlane::PLANE_V) {
        sys = ptrOffset(sys, height * pitch * 2);
        pitch /= 2;
    } else if (plane == ImagePlane::PLANE_U) {
        sys = ptrOffset(sys, height * pitch * 2 + height * pitch / 2);
        pitch /= 2;
    } else if (plane == ImagePlane::PLANE_UV) {
        sys = ptrOffset(sys, height * pitch * 2);
    }
    uint32_t size = pitch * height;

    gmmResourceCopyBLT.Sys.pData = sys;
    gmmResourceCopyBLT.Gpu.pData = gpu;
    gmmResourceCopyBLT.Sys.RowPitch = pitch;
    gmmResourceCopyBLT.Blt.Upload = upload;
    gmmResourceCopyBLT.Sys.BufferSize = size;

    return this->gmmResourceInfo->cpuBlt(&gmmResourceCopyBLT);
}

bool Gmm::unifiedAuxTranslationCapable() const {
    auto gmmFlags = this->gmmResourceInfo->getResourceFlags();
    UNRECOVERABLE_IF(gmmFlags->Info.RenderCompressed && gmmFlags->Info.MediaCompressed);
    return gmmFlags->Gpu.CCS && gmmFlags->Gpu.UnifiedAuxSurface && (gmmFlags->Info.RenderCompressed | gmmFlags->Info.MediaCompressed);
}

bool Gmm::hasMultisampleControlSurface() const {
    return this->gmmResourceInfo->getResourceFlags()->Gpu.MCS;
}

GmmHelper *Gmm::getGmmHelper() const {
    return this->gmmHelper;
}

uint32_t Gmm::getUnifiedAuxPitchTiles() {
    return this->gmmResourceInfo->getRenderAuxPitchTiles();
}
uint32_t Gmm::getAuxQPitch() {
    return this->gmmResourceInfo->getAuxQPitch();
}

void Gmm::applyMemoryFlags(StorageInfo &storageInfo) {
    auto hardwareInfo = gmmHelper->getClientContext()->getHardwareInfo();

    if (hardwareInfo->featureTable.flags.ftrLocalMemory) {
        if (storageInfo.systemMemoryPlacement) {
            resourceParams.Flags.Info.NonLocalOnly = 1;
        } else {
            if (extraMemoryFlagsRequired()) {
                applyExtraMemoryFlags(storageInfo);
            } else if (!storageInfo.isLockable) {
                resourceParams.Flags.Info.NotLockable = 1;
                if (isCompressionEnabled || storageInfo.localOnlyRequired) {
                    resourceParams.Flags.Info.LocalOnly = 1;
                }
            }
        }
    }

    if (hardwareInfo->featureTable.flags.ftrMultiTileArch) {
        resourceParams.MultiTileArch.Enable = 1;
        if (storageInfo.systemMemoryPlacement) {
            resourceParams.MultiTileArch.GpuVaMappingSet = hardwareInfo->gtSystemInfo.MultiTileArchInfo.TileMask;
            resourceParams.MultiTileArch.LocalMemPreferredSet = 0;
            resourceParams.MultiTileArch.LocalMemEligibilitySet = 0;

        } else {
            auto tileSelected = std::max(storageInfo.memoryBanks.to_ulong(), 1lu);

            if (storageInfo.cloningOfPageTables) {
                resourceParams.MultiTileArch.GpuVaMappingSet = static_cast<uint8_t>(storageInfo.pageTablesVisibility.to_ulong());
            } else {
                resourceParams.MultiTileArch.TileInstanced = storageInfo.tileInstanced;
                resourceParams.MultiTileArch.GpuVaMappingSet = static_cast<uint8_t>(tileSelected);
            }

            resourceParams.MultiTileArch.LocalMemPreferredSet = static_cast<uint8_t>(tileSelected);
            resourceParams.MultiTileArch.LocalMemEligibilitySet = static_cast<uint8_t>(tileSelected);
        }
    }
}

void Gmm::applyDebugOverrides() {
    if (-1 != DebugManager.flags.OverrideGmmResourceUsageField.get()) {
        resourceParams.Usage = static_cast<GMM_RESOURCE_USAGE_TYPE>(DebugManager.flags.OverrideGmmResourceUsageField.get());
    }

    if (true == (DebugManager.flags.ForceAllResourcesUncached.get())) {
        resourceParams.Usage = GMM_RESOURCE_USAGE_SURFACE_UNCACHED;
    }
}
} // namespace NEO
