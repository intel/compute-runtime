/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/surface_format_info.h"

namespace NEO {
Gmm::Gmm(GmmClientContext *clientContext, const void *alignedPtr, size_t alignedSize, bool uncacheable) : Gmm(clientContext, alignedPtr, alignedSize, uncacheable, false, true, {}) {}

Gmm::Gmm(GmmClientContext *clientContext, const void *alignedPtr, size_t alignedSize, bool uncacheable, bool preferRenderCompressed, bool systemMemoryPool, StorageInfo storageInfo) : clientContext(clientContext) {
    resourceParams.Type = RESOURCE_BUFFER;
    resourceParams.Format = GMM_FORMAT_GENERIC_8BIT;
    resourceParams.BaseWidth64 = static_cast<uint64_t>(alignedSize);
    resourceParams.BaseHeight = 1;
    resourceParams.Depth = 1;
    if (!uncacheable) {
        resourceParams.Usage = GMM_RESOURCE_USAGE_OCL_BUFFER;
    } else {
        resourceParams.Usage = GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC;
    }
    resourceParams.Flags.Info.Linear = 1;
    resourceParams.Flags.Info.Cacheable = 1;
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

    applyAuxFlagsForBuffer(preferRenderCompressed);
    applyMemoryFlags(systemMemoryPool, storageInfo);

    gmmResourceInfo.reset(GmmResourceInfo::create(clientContext, &resourceParams));
}

Gmm::Gmm(GmmClientContext *clientContext, GMM_RESOURCE_INFO *inputGmm) : clientContext(clientContext) {
    gmmResourceInfo.reset(GmmResourceInfo::create(clientContext, inputGmm));
}

Gmm::~Gmm() = default;

Gmm::Gmm(GmmClientContext *clientContext, ImageInfo &inputOutputImgInfo, StorageInfo storageInfo) : clientContext(clientContext) {
    this->resourceParams = {};
    setupImageResourceParams(inputOutputImgInfo);
    applyMemoryFlags(!inputOutputImgInfo.useLocalMemory, storageInfo);
    this->gmmResourceInfo.reset(GmmResourceInfo::create(clientContext, &this->resourceParams));
    UNRECOVERABLE_IF(this->gmmResourceInfo == nullptr);

    queryImageParams(inputOutputImgInfo);
}

void Gmm::setupImageResourceParams(ImageInfo &imgInfo) {
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

    auto &hwHelper = HwHelper::get(clientContext->getHardwareInfo()->platform.eRenderCoreFamily);

    resourceParams.NoGfxMemory = 1; // dont allocate, only query for params

    resourceParams.Usage = GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE;
    resourceParams.Format = imgInfo.surfaceFormat->GMMSurfaceFormat;
    resourceParams.Flags.Gpu.Texture = 1;
    resourceParams.BaseWidth64 = imageWidth;
    resourceParams.BaseHeight = imageHeight;
    resourceParams.Depth = imageDepth;
    resourceParams.ArraySize = imageCount;
    resourceParams.Flags.Wa.__ForceOtherHVALIGN4 = hwHelper.hvAlign4Required();
    resourceParams.MaxLod = imgInfo.baseMipLevel + imgInfo.mipCount;
    if (imgInfo.imgDesc.imageRowPitch && imgInfo.imgDesc.fromParent) {
        resourceParams.OverridePitch = (uint32_t)imgInfo.imgDesc.imageRowPitch;
        resourceParams.Flags.Info.AllowVirtualPadding = true;
    }

    applyAuxFlagsForImage(imgInfo);
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
    if (clientContext->getHardwareInfo()->platform.eRenderCoreFamily == IGFX_GEN8_CORE && resType == GMM_RESOURCE_TYPE::RESOURCE_3D) {
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

uint32_t Gmm::getUnifiedAuxPitchTiles() {
    return this->gmmResourceInfo->getRenderAuxPitchTiles();
}
uint32_t Gmm::getAuxQPitch() {
    return this->gmmResourceInfo->getAuxQPitch();
}
} // namespace NEO
