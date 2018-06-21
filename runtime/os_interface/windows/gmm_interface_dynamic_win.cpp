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

#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/os_interface/os_library.h"

namespace Os {
extern const char *gmmDllName;
}

namespace OCLRT {
GMM_STATUS(GMM_STDCALL *myPfnCreateSingletonContext)
(const PLATFORM Platform, const SKU_FEATURE_TABLE *pSkuTable, const WA_TABLE *pWaTable, const GT_SYSTEM_INFO *pGtSysInfo);
GMM_STATUS GMM_STDCALL myGmmInitGlobalContext(const PLATFORM Platform, const SKU_FEATURE_TABLE *pSkuTable, const WA_TABLE *pWaTable, const GT_SYSTEM_INFO *pGtSysInfo, GMM_CLIENT ClientType) {
    return myPfnCreateSingletonContext(Platform, pSkuTable, pWaTable, pGtSysInfo);
}
decltype(GmmHelper::initGlobalContextFunc) GmmHelper::initGlobalContextFunc = &myGmmInitGlobalContext;
decltype(GmmHelper::destroyGlobalContextFunc) GmmHelper::destroyGlobalContextFunc = nullptr;
decltype(GmmHelper::createClientContextFunc) GmmHelper::createClientContextFunc = nullptr;
decltype(GmmHelper::deleteClientContextFunc) GmmHelper::deleteClientContextFunc = nullptr;

std::unique_ptr<OsLibrary> gmmLib;
void GmmHelper::loadLib() {
    gmmLib.reset(OsLibrary::load(Os::gmmDllName));

    UNRECOVERABLE_IF(!gmmLib);
    if (gmmLib->isLoaded()) {
        auto openGmmFunc = reinterpret_cast<decltype(&OpenGmm)>(gmmLib->getProcAddress(GMM_ENTRY_NAME));
        GmmExportEntries entries;
        auto status = openGmmFunc(&entries);
        if (status == GMM_SUCCESS) {
            myPfnCreateSingletonContext = entries.pfnCreateSingletonContext;
            GmmHelper::destroyGlobalContextFunc = entries.pfnDestroySingletonContext;
            GmmHelper::createClientContextFunc = entries.pfnCreateClientContext;
            GmmHelper::deleteClientContextFunc = entries.pfnDeleteClientContext;
            isLoaded = myPfnCreateSingletonContext && GmmHelper::destroyGlobalContextFunc && GmmHelper::createClientContextFunc && GmmHelper::deleteClientContextFunc;
        }
    }
    UNRECOVERABLE_IF(!isLoaded);
}
} // namespace OCLRT
