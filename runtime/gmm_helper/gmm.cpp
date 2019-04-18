/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gmm_helper/gmm.h"

#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/gmm_helper/resource_info.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/helpers/surface_formats.h"

namespace NEO {
Gmm::Gmm(const void *alignedPtr, size_t alignedSize, bool uncacheable) : Gmm(alignedPtr, alignedSize, uncacheable, false, true, {}) {}

Gmm::Gmm(const void *alignedPtr, size_t alignedSize, bool uncacheable, bool preferRenderCompressed, bool systemMemoryPool, StorageInfo storageInfo) {
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
        resourceParams.pExistingSysMem = reinterpret_cast<GMM_VOIDPTR64>(alignedPtr);
        resourceParams.ExistingSysMemSize = alignedSize;
    } else {
        resourceParams.NoGfxMemory = 1u;
    }

    if (resourceParams.BaseWidth64 >= GmmHelper::maxPossiblePitch) {
        resourceParams.Flags.Gpu.NoRestriction = 1;
    }

    applyAuxFlagsForBuffer(preferRenderCompressed);
    applyMemoryFlags(systemMemoryPool, storageInfo);

    gmmResourceInfo.reset(GmmResourceInfo::create(&resourceParams));
}

Gmm::Gmm(GMM_RESOURCE_INFO *inputGmm) {
    gmmResourceInfo.reset(GmmResourceInfo::create(inputGmm));
}

Gmm::Gmm(ImageInfo &inputOutputImgInfo) {
    this->resourceParams = {};
    setupImageResourceParams(inputOutputImgInfo);
    this->gmmResourceInfo.reset(GmmResourceInfo::create(&this->resourceParams));
    UNRECOVERABLE_IF(this->gmmResourceInfo == nullptr);

    queryImageParams(inputOutputImgInfo);
}

void Gmm::setupImageResourceParams(ImageInfo &imgInfo) {
    uint64_t imageWidth = static_cast<uint64_t>(imgInfo.imgDesc->image_width);
    uint32_t imageHeight = 1;
    uint32_t imageDepth = 1;
    uint32_t imageCount = 1;

    switch (imgInfo.imgDesc->image_type) {
    case CL_MEM_OBJECT_IMAGE1D:
    case CL_MEM_OBJECT_IMAGE1D_ARRAY:
    case CL_MEM_OBJECT_IMAGE1D_BUFFER:
        resourceParams.Type = GMM_RESOURCE_TYPE::RESOURCE_1D;
        break;
    case CL_MEM_OBJECT_IMAGE2D:
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
        resourceParams.Type = GMM_RESOURCE_TYPE::RESOURCE_2D;
        imageHeight = static_cast<uint32_t>(imgInfo.imgDesc->image_height);
        break;
    case CL_MEM_OBJECT_IMAGE3D:
        resourceParams.Type = GMM_RESOURCE_TYPE::RESOURCE_3D;
        imageHeight = static_cast<uint32_t>(imgInfo.imgDesc->image_height);
        imageDepth = static_cast<uint32_t>(imgInfo.imgDesc->image_depth);
        break;
    default:
        return;
    }

    if (imgInfo.imgDesc->image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY ||
        imgInfo.imgDesc->image_type == CL_MEM_OBJECT_IMAGE2D_ARRAY) {
        imageCount = static_cast<uint32_t>(imgInfo.imgDesc->image_array_size);
    }

    resourceParams.Flags.Info.Linear = 1;

    switch (imgInfo.tilingMode) {
    case TilingMode::DEFAULT:
        if (GmmHelper::allowTiling(*imgInfo.imgDesc)) {
            resourceParams.Flags.Info.TiledY = 1;
        }
        break;
    case TilingMode::TILE_Y:
        resourceParams.Flags.Info.TiledY = 1;
        break;
    case TilingMode::NON_TILED:
        break;
    default:
        UNRECOVERABLE_IF(true);
        break;
    }

    resourceParams.NoGfxMemory = 1; // dont allocate, only query for params

    resourceParams.Usage = GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE;
    resourceParams.Format = imgInfo.surfaceFormat->GMMSurfaceFormat;
    resourceParams.Flags.Gpu.Texture = 1;
    resourceParams.BaseWidth64 = imageWidth;
    resourceParams.BaseHeight = imageHeight;
    resourceParams.Depth = imageDepth;
    resourceParams.ArraySize = imageCount;
    resourceParams.Flags.Wa.__ForceOtherHVALIGN4 = 1;
    resourceParams.MaxLod = imgInfo.baseMipLevel + imgInfo.mipCount;
    if (imgInfo.imgDesc->image_row_pitch && imgInfo.imgDesc->mem_object) {
        resourceParams.OverridePitch = (uint32_t)imgInfo.imgDesc->image_row_pitch;
        resourceParams.Flags.Info.AllowVirtualPadding = true;
    }

    applyAuxFlagsForImage(imgInfo);
    auto &hwHelper = HwHelper::get(GmmHelper::getInstance()->getHardwareInfo()->pPlatform->eRenderCoreFamily);
    if (!hwHelper.supportsYTiling() && resourceParams.Flags.Info.TiledY == 1) {
        resourceParams.Flags.Info.Linear = 0;
        resourceParams.Flags.Info.TiledY = 0;
    }
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
    if (GmmHelper::getInstance()->getHardwareInfo()->pPlatform->eRenderCoreFamily == IGFX_GEN8_CORE && resType == GMM_RESOURCE_TYPE::RESOURCE_3D) {
        return 0;
    }
    return gmmResourceInfo->getQPitch();
}

uint32_t Gmm::getRenderHAlignment() {
    return GmmHelper::getRenderAlignment(gmmResourceInfo->getHAlign());
}

uint32_t Gmm::getRenderVAlignment() {
    return GmmHelper::getRenderAlignment(gmmResourceInfo->getVAlign());
}

void Gmm::updateImgInfo(ImageInfo &imgInfo, cl_image_desc &imgDesc, cl_uint arrayIndex) {
    imgDesc.image_width = gmmResourceInfo->getBaseWidth();
    imgDesc.image_row_pitch = gmmResourceInfo->getRenderPitch();
    if (imgDesc.image_row_pitch == 0) {
        size_t width = alignUp(imgDesc.image_width, gmmResourceInfo->getHAlign());
        imgDesc.image_row_pitch = width * (gmmResourceInfo->getBitsPerPixel() >> 3);
    }
    imgDesc.image_height = gmmResourceInfo->getBaseHeight();
    imgDesc.image_depth = gmmResourceInfo->getBaseDepth();
    imgDesc.image_array_size = gmmResourceInfo->getArraySize();
    if (imgDesc.image_depth > 1 || imgDesc.image_array_size > 1) {
        GMM_REQ_OFFSET_INFO reqOffsetInfo = {};
        reqOffsetInfo.Slice = imgDesc.image_depth > 1 ? 1 : 0;
        reqOffsetInfo.ArrayIndex = imgDesc.image_array_size > 1 ? 1 : 0;
        reqOffsetInfo.ReqLock = 1;
        gmmResourceInfo->getOffset(reqOffsetInfo);
        imgDesc.image_slice_pitch = static_cast<size_t>(reqOffsetInfo.Lock.Offset);
    } else {
        imgDesc.image_slice_pitch = gmmResourceInfo->getSizeAllocation();
    }

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

uint8_t Gmm::resourceCopyBlt(void *sys, void *gpu, uint32_t pitch, uint32_t height, unsigned char upload, OCLPlane plane) {
    GMM_RES_COPY_BLT gmmResourceCopyBLT = {};

    if (plane == OCLPlane::PLANE_V) {
        sys = ptrOffset(sys, height * pitch * 2);
        pitch /= 2;
    } else if (plane == OCLPlane::PLANE_U) {
        sys = ptrOffset(sys, height * pitch * 2 + height * pitch / 2);
        pitch /= 2;
    } else if (plane == OCLPlane::PLANE_UV) {
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
    return gmmFlags->Gpu.CCS && gmmFlags->Gpu.UnifiedAuxSurface && gmmFlags->Info.RenderCompressed;
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
