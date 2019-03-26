/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gmm_helper/resource_info.h"

namespace NEO {
GmmResourceInfo *GmmResourceInfo::create(GMM_RESCREATE_PARAMS *resourceCreateParams) {
    return new GmmResourceInfo(resourceCreateParams);
}

GmmResourceInfo *GmmResourceInfo::create(GMM_RESOURCE_INFO *inputGmmResourceInfo) {
    return new GmmResourceInfo(inputGmmResourceInfo);
}
} // namespace NEO
