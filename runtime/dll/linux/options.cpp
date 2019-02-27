/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/os_library.h"

#include "External/Common/GmmLibDllName.h"
#include "igc.opencl.h"

namespace Os {
// Compiler library names
const char *frontEndDllName = FCL_LIBRARY_NAME;
const char *igcDllName = IGC_LIBRARY_NAME;
const char *libvaDllName = "libva.so.2";
const char *gmmDllName = GMM_UMD_DLL;
const char *gmmEntryName = GMM_ENTRY_NAME;

const char *sysFsPciPath = "/sys/bus/pci/devices/";
const char *tbxLibName = "libtbxAccess.so";
} // namespace Os
