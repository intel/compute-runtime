/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/gmm_helper/resource_info.h"

#include "gmm_client_context.h"

namespace NEO {

GmmResourceInfo::GmmResourceInfo(GMM_RESCREATE_PARAMS *resourceCreateParams) {
    auto resourceInfoPtr = GmmHelper::getClientContext()->createResInfoObject(resourceCreateParams);
    createResourceInfo(resourceInfoPtr);
}

GmmResourceInfo::GmmResourceInfo(GMM_RESOURCE_INFO *inputGmmResourceInfo) {
    auto resourceInfoPtr = GmmHelper::getClientContext()->copyResInfoObject(inputGmmResourceInfo);
    createResourceInfo(resourceInfoPtr);
}

void GmmResourceInfo::createResourceInfo(GMM_RESOURCE_INFO *resourceInfoPtr) {
    auto gmmClientContext = GmmHelper::getClientContext();
    auto customDeleter = [gmmClientContext](GMM_RESOURCE_INFO *gmmResourceInfo) {
        gmmClientContext->destroyResInfoObject(gmmResourceInfo);
    };
    this->resourceInfo = UniquePtrType(resourceInfoPtr, customDeleter);
}

} // namespace NEO
