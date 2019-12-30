/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gmm_helper/resource_info.h"

namespace NEO {
GmmResourceInfo *GmmResourceInfo::create(GmmClientContext *clientContext, GMM_RESCREATE_PARAMS *resourceCreateParams) {
    return new GmmResourceInfo(clientContext, resourceCreateParams);
}

GmmResourceInfo *GmmResourceInfo::create(GmmClientContext *clientContext, GMM_RESOURCE_INFO *inputGmmResourceInfo) {
    return new GmmResourceInfo(clientContext, inputGmmResourceInfo);
}
} // namespace NEO
