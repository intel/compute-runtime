/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_interface.h"
namespace NEO {
namespace GmmInterface {
GMM_STATUS initialize(GMM_INIT_IN_ARGS *pInArgs, GMM_INIT_OUT_ARGS *pOutArgs) {
    return InitializeGmm(pInArgs, pOutArgs);
}

void destroy(GMM_INIT_OUT_ARGS *pInArgs) {
    GmmAdapterDestroy(pInArgs);
}
} // namespace GmmInterface
} // namespace NEO
