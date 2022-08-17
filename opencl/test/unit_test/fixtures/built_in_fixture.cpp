/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/built_in_fixture.h"

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/device/device.h"
#include "shared/test/common/helpers/kernel_binary_helper.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/libult/global_environment.h"
#include "shared/test/common/test_macros/test_checks_shared.h"

using namespace NEO;

void BuiltInFixture::setUp(Device *pDevice) {
    // create an instance of the builtins
    pBuiltIns = pDevice->getBuiltIns();
    pBuiltIns->setCacheingEnableState(false);

    // set mock compiler to return expected kernel...
    MockCompilerDebugVars fclDebugVars;
    MockCompilerDebugVars igcDebugVars;

    std::string builtInsFileName;
    if (TestChecks::supportsImages(pDevice->getHardwareInfo())) {
        builtInsFileName = KernelBinaryHelper::BUILT_INS_WITH_IMAGES;
    } else {
        builtInsFileName = KernelBinaryHelper::BUILT_INS;
    }
    retrieveBinaryKernelFilename(fclDebugVars.fileName, builtInsFileName + "_", ".bc");
    retrieveBinaryKernelFilename(igcDebugVars.fileName, builtInsFileName + "_", ".gen");

    gEnvironment->fclPushDebugVars(fclDebugVars);
    gEnvironment->igcPushDebugVars(igcDebugVars);
}

void BuiltInFixture::tearDown() {
    gEnvironment->igcPopDebugVars();
    gEnvironment->fclPopDebugVars();
}
