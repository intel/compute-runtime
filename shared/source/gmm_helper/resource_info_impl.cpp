/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"

#include "gmm_client_context.h"

namespace NEO {

GmmResourceInfo::GmmResourceInfo(GmmClientContext *clientContext, GMM_RESCREATE_PARAMS *resourceCreateParams) : clientContext(clientContext) {
    auto resourceInfoPtr = clientContext->createResInfoObject(resourceCreateParams);
    createResourceInfo(resourceInfoPtr);
}

GmmResourceInfo::GmmResourceInfo(GmmClientContext *clientContext, GMM_RESOURCE_INFO *inputGmmResourceInfo) : clientContext(clientContext) {
    auto resourceInfoPtr = clientContext->copyResInfoObject(inputGmmResourceInfo);
    createResourceInfo(resourceInfoPtr);
}

void GmmResourceInfo::createResourceInfo(GMM_RESOURCE_INFO *resourceInfoPtr) {
    auto customDeleter = [this](GMM_RESOURCE_INFO *gmmResourceInfo) {
        this->clientContext->destroyResInfoObject(gmmResourceInfo);
    };
    this->resourceInfo = UniquePtrType(resourceInfoPtr, customDeleter);
}

} // namespace NEO
