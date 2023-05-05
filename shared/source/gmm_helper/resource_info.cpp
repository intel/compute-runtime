/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/resource_info.h"

#include "shared/source/helpers/debug_helpers.h"

namespace NEO {
GmmResourceInfo *GmmResourceInfo::create(GmmClientContext *clientContext, GMM_RESCREATE_PARAMS *resourceCreateParams) {
    auto resourceInfo = new GmmResourceInfo(clientContext, resourceCreateParams);
    UNRECOVERABLE_IF(resourceInfo->peekHandle() == 0);
    return resourceInfo;
}

GmmResourceInfo *GmmResourceInfo::create(GmmClientContext *clientContext, GMM_RESOURCE_INFO *inputGmmResourceInfo) {
    auto resourceInfo = new GmmResourceInfo(clientContext, inputGmmResourceInfo);
    UNRECOVERABLE_IF(resourceInfo->peekHandle() == 0);
    return resourceInfo;
}

GmmResourceInfo *GmmResourceInfo::create(GmmClientContext *clientContext, GMM_RESOURCE_INFO *inputGmmResourceInfo, bool openingHandle) {
    auto resourceInfo = new GmmResourceInfo(clientContext, inputGmmResourceInfo, openingHandle);
    UNRECOVERABLE_IF(resourceInfo->peekHandle() == 0);
    return resourceInfo;
}

} // namespace NEO
