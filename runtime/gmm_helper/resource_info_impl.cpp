/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/gmm_helper/resource_info.h"

#include "gmm_client_context.h"

namespace OCLRT {
void GmmResourceInfo::customDeleter(GMM_RESOURCE_INFO *gmmResourceInfo) {
    GmmHelper::getClientContext()->destroyResInfoObject(gmmResourceInfo);
}

GmmResourceInfo::GmmResourceInfo(GMM_RESCREATE_PARAMS *resourceCreateParams) {
    auto resourceInfoPtr = GmmHelper::getClientContext()->createResInfoObject(resourceCreateParams);
    this->resourceInfo = UniquePtrType(resourceInfoPtr, GmmResourceInfo::customDeleter);
}

GmmResourceInfo::GmmResourceInfo(GMM_RESOURCE_INFO *inputGmmResourceInfo) {
    auto resourceInfoPtr = GmmHelper::getClientContext()->copyResInfoObject(inputGmmResourceInfo);
    this->resourceInfo = UniquePtrType(resourceInfoPtr, GmmResourceInfo::customDeleter);
}

} // namespace OCLRT
