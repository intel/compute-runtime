/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/helpers/kernel_binary_helper.h"

#include "opencl/test/unit_test/global_environment.h"
#include "opencl/test/unit_test/helpers/test_files.h"

#include <string>

extern PRODUCT_FAMILY productFamily;

KernelBinaryHelper::KernelBinaryHelper(const std::string &name, bool appendOptionsToFileName) {
    // set mock compiler to return expected kernel
    MockCompilerDebugVars fclDebugVars;
    MockCompilerDebugVars igcDebugVars;

    retrieveBinaryKernelFilename(fclDebugVars.fileName, name + "_", ".bc");
    retrieveBinaryKernelFilename(igcDebugVars.fileName, name + "_", ".gen");

    fclDebugVars.appendOptionsToFileName = appendOptionsToFileName;
    igcDebugVars.appendOptionsToFileName = appendOptionsToFileName;

    gEnvironment->fclPushDebugVars(fclDebugVars);
    gEnvironment->igcPushDebugVars(igcDebugVars);
}

KernelBinaryHelper::~KernelBinaryHelper() {
    gEnvironment->igcPopDebugVars();
    gEnvironment->fclPopDebugVars();
}
