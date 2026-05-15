/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/client_context/gmm_handle_allocator.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/debug_helpers.h"

namespace NEO {

GmmResourceInfo::GmmResourceInfo(GmmClientContext *clientContext, GMM_RESCREATE_PARAMS *resourceCreateParams) : clientContext(clientContext) {
    auto resourceInfoPtr = clientContext->createResInfoObject(resourceCreateParams);
    createResourceInfo(resourceInfoPtr);
}

GmmResourceInfo::GmmResourceInfo(GmmClientContext *clientContext, GMM_RESOURCE_INFO *inputGmmResourceInfo) : GmmResourceInfo(clientContext, inputGmmResourceInfo, false) {}

GmmResourceInfo::GmmResourceInfo(GmmClientContext *clientContext, GMM_RESOURCE_INFO *inputGmmResourceInfo, bool openingHandle) : clientContext(clientContext) {
    if (openingHandle) {
        decodeResourceInfo(inputGmmResourceInfo);
    } else {
        auto resourceInfoPtr = clientContext->copyResInfoObject(inputGmmResourceInfo);
        createResourceInfo(resourceInfoPtr);
    }
}

void GmmResourceInfo::decodeResourceInfo(GMM_RESOURCE_INFO *inputGmmResourceInfo) {
    auto customDeleter = [this](GMM_RESOURCE_INFO *gmmResourceInfo) {
        this->clientContext->destroyResInfoObject(gmmResourceInfo);
    };

    UNRECOVERABLE_IF(this->handle != nullptr);

    auto resourceInfoPtr = this->clientContext->copyResInfoObject(inputGmmResourceInfo);
    this->resourceInfo = UniquePtrType(resourceInfoPtr, customDeleter);

    this->handle = this->clientContext->getHandleAllocator()->createHandle(inputGmmResourceInfo);
    this->handleSize = this->clientContext->getHandleAllocator()->getHandleSize();

    this->clientContext->getHandleAllocator()->openHandle(inputGmmResourceInfo, this->resourceInfo.get(), this->clientContext->getHandleAllocator()->getHandleSize());
}

void GmmResourceInfo::createResourceInfo(GMM_RESOURCE_INFO *resourceInfoPtr) {
    auto customDeleter = [this](GMM_RESOURCE_INFO *gmmResourceInfo) {
        this->clientContext->destroyResInfoObject(gmmResourceInfo);
    };
    this->resourceInfo = UniquePtrType(resourceInfoPtr, customDeleter);

    if (this->clientContext->getHandleAllocator()) {
        this->handle = this->clientContext->getHandleAllocator()->createHandle(this->resourceInfo.get());
        this->handleSize = this->clientContext->getHandleAllocator()->getHandleSize();
    } else {
        this->handle = this->resourceInfo.get();
        this->handleSize = sizeof(GMM_RESOURCE_INFO);
    }
}

void GmmResourceInfo::refreshHandle() {
    if (this->clientContext && this->clientContext->getHandleAllocator()) {
        this->clientContext->getHandleAllocator()->openHandle(this->handle, this->resourceInfo.get(), this->clientContext->getHandleAllocator()->getHandleSize());
    }
}

size_t GmmResourceInfo::getSizeAllocation() { return static_cast<size_t>(resourceInfo->GetSize(GMM_TOTAL_SURF)); }

size_t GmmResourceInfo::getBaseWidth() { return static_cast<size_t>(resourceInfo->GetBaseWidth()); }

size_t GmmResourceInfo::getBaseHeight() { return static_cast<size_t>(resourceInfo->GetBaseHeight()); }

size_t GmmResourceInfo::getBaseDepth() { return static_cast<size_t>(resourceInfo->GetBaseDepth()); }

size_t GmmResourceInfo::getArraySize() { return static_cast<size_t>(resourceInfo->GetArraySize()); }

size_t GmmResourceInfo::getRenderPitch() { return static_cast<size_t>(resourceInfo->GetRenderPitch()); }

uint32_t GmmResourceInfo::getNumSamples() { return resourceInfo->GetNumSamples(); }

uint32_t GmmResourceInfo::getQPitch() { return resourceInfo->GetQPitch(); }

uint32_t GmmResourceInfo::getBitsPerPixel() { return resourceInfo->GetBitsPerPixel(); }

uint32_t GmmResourceInfo::getHAlign() { return resourceInfo->GetHAlign(); }

uint32_t GmmResourceInfo::getHAlignSurfaceState() { return resourceInfo->GetHAlignSurfaceState(); }

uint32_t GmmResourceInfo::getVAlignSurfaceState() { return resourceInfo->GetVAlignSurfaceState(); }

uint32_t GmmResourceInfo::getMaxLod() { return resourceInfo->GetMaxLod(); }

uint32_t GmmResourceInfo::getTileModeSurfaceState() { return resourceInfo->GetTileModeSurfaceState(); }

uint32_t GmmResourceInfo::getRenderAuxPitchTiles() { return resourceInfo->GetRenderAuxPitchTiles(); }

uint32_t GmmResourceInfo::getAuxQPitch() { return resourceInfo->GetAuxQPitch(); }

uint32_t GmmResourceInfo::getMipTailStartLODSurfaceState() { return resourceInfo->GetMipTailStartLodSurfaceState(); }

GMM_RESOURCE_FORMAT GmmResourceInfo::getResourceFormat() { return resourceInfo->GetResourceFormat(); }

GMM_SURFACESTATE_FORMAT GmmResourceInfo::getResourceFormatSurfaceState() { return resourceInfo->GetResourceFormatSurfaceState(); }

GMM_RESOURCE_TYPE GmmResourceInfo::getResourceType() { return resourceInfo->GetResourceType(); }

GMM_RESOURCE_FLAG *GmmResourceInfo::getResourceFlags() { return &resourceInfo->GetResFlags(); }

GMM_STATUS GmmResourceInfo::getOffset(GMM_REQ_OFFSET_INFO &reqOffsetInfo) { return resourceInfo->GetOffset(reqOffsetInfo); }

GMM_RESOURCE_USAGE_TYPE GmmResourceInfo::getCachePolicyUsage() const { return resourceInfo->GetCachePolicyUsage(); }

uint8_t GmmResourceInfo::cpuBlt(GMM_RES_COPY_BLT *resCopyBlt) { return resourceInfo->CpuBlt(resCopyBlt); }

uint64_t GmmResourceInfo::getUnifiedAuxSurfaceOffset(GMM_UNIFIED_AUX_TYPE auxType) { return resourceInfo->GetUnifiedAuxSurfaceOffset(auxType); }

void *GmmResourceInfo::getSystemMemPointer() { return resourceInfo->GetSystemMemPointer(true); }

bool GmmResourceInfo::is64KBPageSuitable() const { return resourceInfo->Is64KBPageSuitable(); }

GMM_RESOURCE_INFO *GmmResourceInfo::peekGmmResourceInfo() const { return resourceInfo.get(); }

GmmResourceInfo::~GmmResourceInfo() {
    if (this->clientContext && this->clientContext->getHandleAllocator()) {
        this->clientContext->getHandleAllocator()->destroyHandle(this->handle);
    }
}

} // namespace NEO
