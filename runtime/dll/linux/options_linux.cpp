/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/os_interface/os_library.h"

#include "External/Common/GmmLibDllName.h"
#include "igc.opencl.h"

namespace Os {
// Compiler library names
const char *frontEndDllName = FCL_LIBRARY_NAME;
const char *igcDllName = IGC_LIBRARY_NAME;
const char *libvaDllName = "libva.so.2";
const char *gmmDllName = GMM_UMD_DLL;
const char *gmmInitFuncName = GMM_ADAPTER_INIT_NAME;
const char *gmmDestroyFuncName = GMM_ADAPTER_DESTROY_NAME;

const char *sysFsPciPath = "/sys/bus/pci/devices/";
const char *tbxLibName = "libtbxAccess.so";

// Os specific Metrics Library name
#if __x86_64__ || __ppc64__
const char *metricsLibraryDllName = "libigdml64.so";
#else
const char *metricsLibraryDllName = "libigdml32.so";
#endif
} // namespace Os
