/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gmm_helper/gmm_helper.h"
#include "core/helpers/debug_helpers.h"
#include "core/os_interface/os_library.h"

namespace Os {
extern const char *gmmDllName;
extern const char *gmmInitFuncName;
extern const char *gmmDestroyFuncName;
} // namespace Os

namespace NEO {

void GmmHelper::loadLib() {
    gmmLib.reset(OsLibrary::load(Os::gmmDllName));
    UNRECOVERABLE_IF(!gmmLib);
    initGmmFunc = reinterpret_cast<decltype(&InitializeGmm)>(gmmLib->getProcAddress(Os::gmmInitFuncName));
    destroyGmmFunc = reinterpret_cast<decltype(&GmmAdapterDestroy)>(gmmLib->getProcAddress(Os::gmmDestroyFuncName));
    UNRECOVERABLE_IF(!initGmmFunc || !destroyGmmFunc);
}
} // namespace NEO
