/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gmm_helper/gmm_interface.h"

namespace NEO {
GMM_INIT_IN_ARGS passedInputArgs = {};
SKU_FEATURE_TABLE passedFtrTable = {};
WA_TABLE passedWaTable = {};
bool copyInputArgs = false;

namespace GmmInterface {
GMM_STATUS initialize(GMM_INIT_IN_ARGS *pInArgs, GMM_INIT_OUT_ARGS *pOutArgs) {
    pOutArgs->pGmmClientContext = reinterpret_cast<GMM_CLIENT_CONTEXT *>(0x01);
    if (pInArgs) {
        if (pInArgs->Platform.eProductFamily == PRODUCT_FAMILY::IGFX_UNKNOWN &&
            pInArgs->Platform.ePCHProductFamily == PCH_PRODUCT_FAMILY::PCH_UNKNOWN) {
            return GMM_ERROR;
        }
        if (copyInputArgs) {
            passedInputArgs = *pInArgs;
            passedFtrTable = *reinterpret_cast<SKU_FEATURE_TABLE *>(pInArgs->pSkuTable);
            passedWaTable = *reinterpret_cast<WA_TABLE *>(pInArgs->pWaTable);
        }
        return GMM_SUCCESS;
    }
    return GMM_INVALIDPARAM;
}

void destroy(GMM_INIT_OUT_ARGS *pInArgs) {
}
} // namespace GmmInterface
} // namespace NEO
