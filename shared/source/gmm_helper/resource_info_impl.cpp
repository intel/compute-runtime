/*
 * Copyright (C) 2018-2021 Intel Corporation
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

GmmResourceInfo::GmmResourceInfo(GmmClientContext *clientContext, GMM_RESOURCE_INFO *inputGmmResourceInfo) : clientContext(clientContext) {
    auto resourceInfoPtr = clientContext->copyResInfoObject(inputGmmResourceInfo);
    createResourceInfo(resourceInfoPtr);
}

GmmResourceInfo::~GmmResourceInfo() {
    if (this->clientContext && this->clientContext->getHandleAllocator()) {
        this->clientContext->getHandleAllocator()->destroyHandle(this->handle);
    }
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

} // namespace NEO
