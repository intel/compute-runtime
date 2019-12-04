/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gmm_helper/gmm_helper.h"
#include "runtime/gmm_helper/resource_info.h"
#include "runtime/platform/platform.h"

#include "gmm_client_context.h"

namespace NEO {

GmmResourceInfo::GmmResourceInfo(GMM_RESCREATE_PARAMS *resourceCreateParams) {
    auto resourceInfoPtr = platform()->peekGmmClientContext()->createResInfoObject(resourceCreateParams);
    createResourceInfo(resourceInfoPtr);
}

GmmResourceInfo::GmmResourceInfo(GMM_RESOURCE_INFO *inputGmmResourceInfo) {
    auto resourceInfoPtr = platform()->peekGmmClientContext()->copyResInfoObject(inputGmmResourceInfo);
    createResourceInfo(resourceInfoPtr);
}

void GmmResourceInfo::createResourceInfo(GMM_RESOURCE_INFO *resourceInfoPtr) {
    auto gmmClientContext = platform()->peekGmmClientContext();
    auto customDeleter = [gmmClientContext](GMM_RESOURCE_INFO *gmmResourceInfo) {
        gmmClientContext->destroyResInfoObject(gmmResourceInfo);
    };
    this->resourceInfo = UniquePtrType(resourceInfoPtr, customDeleter);
}

} // namespace NEO
