/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"

namespace NEO {

GmmResourceInfo::GmmResourceInfo(GmmClientContext *clientContext, GMM_RESCREATE_PARAMS *resourceCreateParams) : clientContext(clientContext) {
    auto resourceInfoPtr = clientContext->createResInfoObject(resourceCreateParams);
    createResourceInfo(resourceInfoPtr);
}

GmmResourceInfo::GmmResourceInfo(GmmClientContext *clientContext, GMM_RESOURCE_INFO *inputGmmResourceInfo) : GmmResourceInfo(clientContext, inputGmmResourceInfo, false) {}

GmmResourceInfo::GmmResourceInfo(GmmClientContext *clientContext, GMM_RESOURCE_INFO *inputGmmResourceInfo, bool openingHandle) : clientContext(clientContext) {
    auto resourceInfoPtr = clientContext->copyResInfoObject(inputGmmResourceInfo);
    if (openingHandle) {
        decodeResourceInfo(resourceInfoPtr, inputGmmResourceInfo);
    } else {
        createResourceInfo(resourceInfoPtr);
    }
}

void GmmResourceInfo::decodeResourceInfo(GMM_RESOURCE_INFO *resourceInfoPtr, GMM_RESOURCE_INFO *inputGmmResourceInfo) {
    auto customDeleter = [this](GMM_RESOURCE_INFO *gmmResourceInfo) {
        this->clientContext->destroyResInfoObject(gmmResourceInfo);
    };
    if (this->clientContext->getHandleAllocator()) {
        this->resourceInfo = UniquePtrType(resourceInfoPtr, [](GMM_RESOURCE_INFO *gmmResourceInfo) {});
        this->clientContext->getHandleAllocator()->openHandle(inputGmmResourceInfo, resourceInfoPtr, this->clientContext->getHandleAllocator()->getHandleSize());
        this->handle = resourceInfoPtr;
        this->handleSize = this->clientContext->getHandleAllocator()->getHandleSize();
        return;
    }
    this->resourceInfo = UniquePtrType(resourceInfoPtr, customDeleter);
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
    if (this->clientContext) {
        this->decodeResourceInfo(this->resourceInfo.release(), static_cast<GMM_RESOURCE_INFO *>(this->handle));
    }
}

GmmResourceInfo::~GmmResourceInfo() {
    if (this->clientContext && this->clientContext->getHandleAllocator()) {
        this->clientContext->getHandleAllocator()->destroyHandle(this->handle);
    }
}

} // namespace NEO
