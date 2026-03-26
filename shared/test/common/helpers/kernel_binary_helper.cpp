/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/kernel_binary_helper.h"

const std::string KernelBinaryHelper::BUILT_INS("all_builtins");
const std::string KernelBinaryHelper::BUILT_INS_WITH_IMAGES("all_builtins_with_images");

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
