/*
* Copyright (c) 2017 - 2018, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/

#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/gmm_helper/resource_info.h"
#include "runtime/helpers/get_info.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/helpers/surface_formats.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/sku_info/operations/sku_info_transfer.h"

namespace OCLRT {
void Gmm::create() {
    if (resourceParams.BaseWidth >= maxPossiblePitch) {
        resourceParams.Flags.Gpu.NoRestriction = 1;
    }

    gmmResourceInfo.reset(GmmResourceInfo::create(&resourceParams));
}

bool Gmm::initContext(const PLATFORM *pPlatform,
                      const FeatureTable *pSkuTable,
                      const WorkaroundTable *pWaTable,
                      const GT_SYSTEM_INFO *pGtSysInfo) {
    if (!Gmm::gmmClientContext) {
        _SKU_FEATURE_TABLE gmmFtrTable = {};
        _WA_TABLE gmmWaTable = {};
        SkuInfoTransfer::transferFtrTableForGmm(&gmmFtrTable, pSkuTable);
        SkuInfoTransfer::transferWaTableForGmm(&gmmWaTable, pWaTable);
        if (!isLoaded) {
            loadLib();
        }
        bool success = GMM_SUCCESS == initGlobalContextFunc(*pPlatform, &gmmFtrTable, &gmmWaTable, pGtSysInfo, GMM_CLIENT::GMM_OCL_VISTA);
        UNRECOVERABLE_IF(!success);
        Gmm::gmmClientContext = createClientContextFunc(GMM_CLIENT::GMM_OCL_VISTA);
    }
    return Gmm::gmmClientContext != nullptr;
}

void Gmm::destroyContext() {
    if (Gmm::gmmClientContext) {
        deleteClientContextFunc(Gmm::gmmClientContext);
        Gmm::gmmClientContext = nullptr;
        destroyGlobalContextFunc();
    }
}

uint32_t Gmm::getMOCS(uint32_t type) {
    if (useSimplifiedMocsTable) {
        if (type == GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED) {
            return cacheDisabledIndex;
        } else {
            return cacheEnabledIndex;
        }
    }

    MEMORY_OBJECT_CONTROL_STATE mocs = Gmm::gmmClientContext->CachePolicyGetMemoryObject(nullptr, static_cast<GMM_RESOURCE_USAGE_TYPE>(type));

    return static_cast<uint32_t>(mocs.DwordValue);
}

Gmm *Gmm::create(const void *alignedPtr, size_t alignedSize, bool uncacheable) {
    auto gmm = new Gmm();

    gmm->resourceParams.Type = RESOURCE_BUFFER;
    gmm->resourceParams.Format = GMM_FORMAT_GENERIC_8BIT;
    gmm->resourceParams.BaseWidth = (uint32_t)alignedSize;
    gmm->resourceParams.BaseHeight = 1;
    gmm->resourceParams.Depth = 1;
    if (!uncacheable) {
        gmm->resourceParams.Usage = GMM_RESOURCE_USAGE_OCL_BUFFER;
    } else {
        gmm->resourceParams.Usage = GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC;
    }
    gmm->resourceParams.Flags.Info.Linear = 1;
    gmm->resourceParams.Flags.Info.Cacheable = 1;
    gmm->resourceParams.Flags.Gpu.Texture = 1;

    if (alignedPtr != nullptr) {
        gmm->resourceParams.Flags.Info.ExistingSysMem = 1;
        gmm->resourceParams.pExistingSysMem = reinterpret_cast<GMM_VOIDPTR64>(alignedPtr);
        gmm->resourceParams.ExistingSysMemSize = alignedSize;
    }

    gmm->create();

    return gmm;
}

Gmm *Gmm::create(GMM_RESOURCE_INFO *inputGmm) {
    auto gmm = new Gmm();
    gmm->gmmResourceInfo.reset(GmmResourceInfo::create(inputGmm));
    return gmm;
}

Gmm *Gmm::createGmmAndQueryImgParams(ImageInfo &imgInfo, const HardwareInfo &hwInfo) {
    Gmm *gmm = new Gmm();
    gmm->queryImageParams(imgInfo, hwInfo);
    return gmm;
}

void Gmm::queryImageParams(ImageInfo &imgInfo, const HardwareInfo &hwInfo) {
    uint32_t imageWidth = static_cast<uint32_t>(imgInfo.imgDesc->image_width);
    uint32_t imageHeight = 1;
    uint32_t imageDepth = 1;
    uint32_t imageCount = 1;

    switch (imgInfo.imgDesc->image_type) {
    case CL_MEM_OBJECT_IMAGE1D:
    case CL_MEM_OBJECT_IMAGE1D_ARRAY:
    case CL_MEM_OBJECT_IMAGE1D_BUFFER:
        this->resourceParams.Type = GMM_RESOURCE_TYPE::RESOURCE_1D;
        break;
    case CL_MEM_OBJECT_IMAGE2D:
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
        this->resourceParams.Type = GMM_RESOURCE_TYPE::RESOURCE_2D;
        imageHeight = static_cast<uint32_t>(imgInfo.imgDesc->image_height);
        break;
    case CL_MEM_OBJECT_IMAGE3D:
        this->resourceParams.Type = GMM_RESOURCE_TYPE::RESOURCE_3D;
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

    this->resourceParams.Flags.Info.Linear = 1;
    if (allowTiling(*imgInfo.imgDesc)) {
        this->resourceParams.Flags.Info.TiledY = 1;
    }

    this->resourceParams.NoGfxMemory = 1; // dont allocate, only query for params

    this->resourceParams.Usage = GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE;
    this->resourceParams.Format = imgInfo.surfaceFormat->GMMSurfaceFormat;
    this->resourceParams.Flags.Gpu.Texture = 1;
    this->resourceParams.BaseWidth = imageWidth;
    this->resourceParams.BaseHeight = imageHeight;
    this->resourceParams.Depth = imageDepth;
    this->resourceParams.ArraySize = imageCount;
    this->resourceParams.Flags.Wa.__ForceOtherHVALIGN4 = 1;
    this->resourceParams.MaxLod = imgInfo.baseMipLevel + imgInfo.mipCount;
    if (imgInfo.imgDesc->image_row_pitch && imgInfo.imgDesc->mem_object) {
        this->resourceParams.OverridePitch = (uint32_t)imgInfo.imgDesc->image_row_pitch;
        this->resourceParams.Flags.Info.AllowVirtualPadding = true;
    }

    applyAuxFlags(imgInfo, hwInfo);

    this->gmmResourceInfo.reset(GmmResourceInfo::create(&this->resourceParams));

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

    if (imgInfo.surfaceFormat->GMMSurfaceFormat == GMM_FORMAT_NV12) {
        GMM_REQ_OFFSET_INFO reqOffsetInfo = {};
        reqOffsetInfo.ReqLock = 1;
        reqOffsetInfo.Slice = 1;
        reqOffsetInfo.ArrayIndex = 0;
        reqOffsetInfo.Plane = GMM_PLANE_U;
        this->gmmResourceInfo->getOffset(reqOffsetInfo);
        imgInfo.yOffsetForUVPlane = reqOffsetInfo.Lock.Offset / reqOffsetInfo.Lock.Pitch;
    }

    imgInfo.qPitch = queryQPitch(hwInfo.pPlatform->eRenderCoreFamily, this->resourceParams.Type);
    return;
}

void Gmm::queryImgFromBufferParams(ImageInfo &imgInfo, GraphicsAllocation *gfxAlloc) {
    // 1D or 2D from buffer
    if (imgInfo.imgDesc->image_row_pitch > 0) {
        imgInfo.rowPitch = imgInfo.imgDesc->image_row_pitch;
    } else {
        imgInfo.rowPitch = getValidParam(imgInfo.imgDesc->image_width) * imgInfo.surfaceFormat->ImageElementSizeInBytes;
    }
    imgInfo.slicePitch = imgInfo.rowPitch * getValidParam(imgInfo.imgDesc->image_height);
    imgInfo.size = gfxAlloc->getUnderlyingBufferSize();
    imgInfo.qPitch = 0;
}

bool Gmm::allowTiling(const cl_image_desc &imageDesc) {
    // consider returning tiling type instead of bool
    if (DebugManager.flags.ForceLinearImages.get()) {
        return false;
    } else {
        auto imageType = imageDesc.image_type;

        if (imageType == CL_MEM_OBJECT_IMAGE1D ||
            imageType == CL_MEM_OBJECT_IMAGE1D_ARRAY ||
            imageType == CL_MEM_OBJECT_IMAGE1D_BUFFER ||
            ((imageType == CL_MEM_OBJECT_IMAGE2D && imageDesc.buffer))) {
            return false;
        } else {
            return true;
        }
    }
}

uint64_t Gmm::canonize(uint64_t address) {
    return ((int64_t)((address & 0xFFFFFFFFFFFF) << (64 - 48))) >> (64 - 48);
}

uint64_t Gmm::decanonize(uint64_t address) {
    return (uint64_t)(address & 0xFFFFFFFFFFFF);
}

uint32_t Gmm::queryQPitch(GFXCORE_FAMILY gfxFamily, GMM_RESOURCE_TYPE resType) {
    if (gfxFamily == IGFX_GEN8_CORE && resType == GMM_RESOURCE_TYPE::RESOURCE_3D) {
        return 0;
    }
    return gmmResourceInfo->getQPitch();
}

uint32_t Gmm::getRenderHAlignment() {
    return getRenderAlignment(gmmResourceInfo->getHAlign());
}

uint32_t Gmm::getRenderVAlignment() {
    return getRenderAlignment(gmmResourceInfo->getVAlign());
}

uint32_t Gmm::getRenderAlignment(uint32_t alignment) {
    uint32_t returnAlign = 0;
    if (alignment == 8) {
        returnAlign = 2;
    } else if (alignment == 16) {
        returnAlign = 3;
    } else {
        returnAlign = 1;
    }
    return returnAlign;
}

uint32_t Gmm::getRenderTileMode(uint32_t tileWalk) {
    uint32_t tileMode = 0; // TILE_MODE_LINEAR
    if (tileWalk == 0) {
        tileMode = 2; // TILE_MODE_XMAJOR;
    } else if (tileWalk == 1) {
        tileMode = 3; // TILE_MODE_YMAJOR;
    } else if (tileWalk == 2) {
        tileMode = 1; // TILE_MODE_WMAJOR;
    }
    return tileMode;
}

uint32_t Gmm::getRenderMultisamplesCount(uint32_t numSamples) {
    if (numSamples == 2) {
        return 1;
    } else if (numSamples == 4) {
        return 2;
    } else if (numSamples == 8) {
        return 3;
    } else if (numSamples == 16) {
        return 4;
    }
    return 0;
}

GMM_YUV_PLANE Gmm::convertPlane(OCLPlane oclPlane) {
    if (oclPlane == OCLPlane::PLANE_Y) {
        return GMM_PLANE_Y;
    } else if (oclPlane == OCLPlane::PLANE_U || oclPlane == OCLPlane::PLANE_UV) {
        return GMM_PLANE_U;
    } else if (oclPlane == OCLPlane::PLANE_V) {
        return GMM_PLANE_V;
    }

    return GMM_NO_PLANE;
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

bool Gmm::useSimplifiedMocsTable = false;
GMM_CLIENT_CONTEXT *Gmm::gmmClientContext = nullptr;
bool Gmm::isLoaded = false;

} // namespace OCLRT
