/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gmm_helper/gmm_helper.h"

namespace OCLRT {
GMM_STATUS GMM_STDCALL myPfnCreateSingletonContext(const PLATFORM platform, const void *pSkuTable, const void *pWaTable, const void *pGtSysInfo) {
    return GmmInitGlobalContext(platform, reinterpret_cast<const SKU_FEATURE_TABLE *>(pSkuTable), reinterpret_cast<const WA_TABLE *>(pWaTable), reinterpret_cast<const GT_SYSTEM_INFO *>(pGtSysInfo), GMM_CLIENT::GMM_OCL_VISTA);
}

void GmmHelper::loadLib() {
    gmmEntries.pfnCreateSingletonContext = myPfnCreateSingletonContext;
    gmmEntries.pfnDestroySingletonContext = GmmDestroyGlobalContext;
    gmmEntries.pfnCreateClientContext = GmmCreateClientContext;
    gmmEntries.pfnDeleteClientContext = GmmDeleteClientContext;
}
} // namespace OCLRT
