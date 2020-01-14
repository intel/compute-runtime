/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gmm_helper/gmm_lib.h"
#include "core/helpers/debug_helpers.h"
#include "core/os_interface/os_library.h"

#include <memory>

using namespace NEO;

static std::unique_ptr<OsLibrary> gmmLib;

GMM_STATUS GMM_STDCALL InitializeGmm(GMM_INIT_IN_ARGS *pInArgs, GMM_INIT_OUT_ARGS *pOutArgs) {
    if (!gmmLib) {
        gmmLib.reset(OsLibrary::load(GMM_UMD_DLL));
        UNRECOVERABLE_IF(!gmmLib);
    }
    auto initGmmFunc = reinterpret_cast<decltype(&InitializeGmm)>(gmmLib->getProcAddress(GMM_ADAPTER_INIT_NAME));
    UNRECOVERABLE_IF(!initGmmFunc);
    return initGmmFunc(pInArgs, pOutArgs);
}

void GMM_STDCALL GmmAdapterDestroy(GMM_INIT_OUT_ARGS *pInArgs) {
    auto destroyGmmFunc = reinterpret_cast<decltype(&GmmAdapterDestroy)>(gmmLib->getProcAddress(GMM_ADAPTER_DESTROY_NAME));
    UNRECOVERABLE_IF(!destroyGmmFunc);
    destroyGmmFunc(pInArgs);
}
