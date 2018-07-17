/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "GmmLib.h"

GMM_CLIENT_CONTEXT *GMM_STDCALL createClientContext(GMM_CLIENT ClientType) {
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
GMM_STATUS GMM_STDCALL createSingletonContext(const PLATFORM Platform,
                                              const void *pSkuTable,
                                              const void *pWaTable,
                                              const void *pGtSysInfo) {
    return GMM_SUCCESS;
}
#endif
#ifdef __cplusplus
extern "C" {
#endif
GMM_STATUS GMM_STDCALL openMockGmm(GmmExportEntries *pm_GmmFuncs) {
    pm_GmmFuncs->pfnCreateClientContext = &createClientContext;
    pm_GmmFuncs->pfnCreateSingletonContext = &createSingletonContext;
    pm_GmmFuncs->pfnDeleteClientContext = &deleteClientContext;
    pm_GmmFuncs->pfnDestroySingletonContext = &destroySingletonContext;
    return GMM_SUCCESS;
}
#ifdef __cplusplus
}
#endif
