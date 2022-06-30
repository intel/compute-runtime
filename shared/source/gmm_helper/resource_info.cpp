/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/resource_info.h"

namespace NEO {
GmmResourceInfo *GmmResourceInfo::create(GmmClientContext *clientContext, GMM_RESCREATE_PARAMS *resourceCreateParams) {
    return new GmmResourceInfo(clientContext, resourceCreateParams);
}

GmmResourceInfo *GmmResourceInfo::create(GmmClientContext *clientContext, GMM_RESOURCE_INFO *inputGmmResourceInfo) {
    return new GmmResourceInfo(clientContext, inputGmmResourceInfo);
}

GmmResourceInfo *GmmResourceInfo::create(GmmClientContext *clientContext, GMM_RESOURCE_INFO *inputGmmResourceInfo, bool openingHandle) {
    return new GmmResourceInfo(clientContext, inputGmmResourceInfo, openingHandle);
}

} // namespace NEO
