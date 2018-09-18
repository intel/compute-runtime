/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/global_environment.h"
#include "unit_tests/helpers/kernel_binary_helper.h"
#include "unit_tests/helpers/test_files.h"

#include <string>

extern PRODUCT_FAMILY productFamily;

const std::string KernelBinaryHelper::BUILT_INS("17246368129176860573");

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
