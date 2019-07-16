/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "GmmLib.h"

GMM_CLIENT_CONTEXT *GMM_STDCALL createClientContext(GMM_CLIENT clientType) {
    return reinterpret_cast<GMM_CLIENT_CONTEXT *>(0x1);
}
void GMM_STDCALL deleteClientContext(GMM_CLIENT_CONTEXT *pGmmClientContext) {
}
void GMM_STDCALL destroySingletonContext(void) {
}
#ifdef _WIN32
GMM_STATUS GMM_STDCALL createSingletonContext(const PLATFORM Platform,
                                              const SKU_FEATURE_TABLE *featureTable,
                                              const WA_TABLE *workaroundTable,
                                              const GT_SYSTEM_INFO *pGtSysInfo) {
    if (Platform.eProductFamily == PRODUCT_FAMILY::IGFX_UNKNOWN &&
        Platform.eRenderCoreFamily == GFXCORE_FAMILY::IGFX_UNKNOWN_CORE &&
        Platform.ePCHProductFamily == PCH_PRODUCT_FAMILY::PCH_UNKNOWN) {
        return GMM_ERROR;
    }
    return GMM_SUCCESS;
}
#else
GMM_STATUS GMM_STDCALL createSingletonContext(const PLATFORM platform,
                                              const void *featureTable,
                                              const void *workaroundTable,
                                              const void *pGtSysInfo) {
    return GMM_SUCCESS;
}
#endif
#ifdef __cplusplus
extern "C" {
#endif
GMM_STATUS GMM_STDCALL openMockGmm(GmmExportEntries *pGmmFuncs) {
    pGmmFuncs->pfnCreateClientContext = &createClientContext;
    pGmmFuncs->pfnCreateSingletonContext = &createSingletonContext;
    pGmmFuncs->pfnDeleteClientContext = &deleteClientContext;
    pGmmFuncs->pfnDestroySingletonContext = &destroySingletonContext;
    return GMM_SUCCESS;
}
#ifdef __cplusplus
}
#endif
