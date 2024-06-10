/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/kernel_binary_helper.h"

#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/libult/global_environment.h"

#include <string>

extern PRODUCT_FAMILY productFamily;

KernelBinaryHelper::KernelBinaryHelper(const std::string &name, bool appendOptionsToFileName) {
    // set mock compiler to return expected kernel
    MockCompilerDebugVars fclDebugVars;
    MockCompilerDebugVars igcDebugVars;

    retrieveBinaryKernelFilename(fclDebugVars.fileName, name + "_", ".spv");
    retrieveBinaryKernelFilename(igcDebugVars.fileName, name + "_", ".bin");

    appendBinaryNameSuffix(fclDebugVars.fileNameSuffix);
    appendBinaryNameSuffix(igcDebugVars.fileNameSuffix);

    fclDebugVars.appendOptionsToFileName = appendOptionsToFileName;
    igcDebugVars.appendOptionsToFileName = appendOptionsToFileName;

    gEnvironment->fclPushDebugVars(fclDebugVars);
    gEnvironment->igcPushDebugVars(igcDebugVars);
}

KernelBinaryHelper::~KernelBinaryHelper() {
    gEnvironment->igcPopDebugVars();
    gEnvironment->fclPopDebugVars();
}
