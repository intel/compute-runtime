/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "GmmLib.h"

#ifdef __cplusplus
extern "C" {
#endif

GMM_STATUS GMM_STDCALL initMockGmm(GMM_INIT_IN_ARGS *pInArgs, GMM_INIT_OUT_ARGS *pOutArgs) {
    pOutArgs->pGmmClientContext = reinterpret_cast<GMM_CLIENT_CONTEXT *>(0x01);
    if (pInArgs->Platform.eProductFamily == PRODUCT_FAMILY::IGFX_UNKNOWN &&
        pInArgs->Platform.ePCHProductFamily == PCH_PRODUCT_FAMILY::PCH_UNKNOWN) {
        return GMM_ERROR;
    }
    return GMM_SUCCESS;
}

void GMM_STDCALL destroyMockGmm(GMM_INIT_OUT_ARGS *pInArgs) {
}

#ifdef __cplusplus
}
#endif
