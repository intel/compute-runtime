/*
 * Copyright (C) 2018 Intel Corporation
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
                                              const SKU_FEATURE_TABLE *pSkuTable,
                                              const WA_TABLE *pWaTable,
                                              const GT_SYSTEM_INFO *pGtSysInfo) {
    return GMM_SUCCESS;
}
#else
GMM_STATUS GMM_STDCALL createSingletonContext(const PLATFORM platform,
                                              const void *pSkuTable,
                                              const void *pWaTable,
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
