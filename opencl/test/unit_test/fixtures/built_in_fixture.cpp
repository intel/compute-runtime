/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/built_in_fixture.h"

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/device/device.h"

#include "opencl/test/unit_test/global_environment.h"
#include "opencl/test/unit_test/helpers/kernel_binary_helper.h"
#include "opencl/test/unit_test/helpers/test_files.h"

using namespace NEO;

BuiltInFixture::BuiltInFixture() : pBuiltIns(nullptr) {
}

void BuiltInFixture::SetUp(Device *pDevice) {
    // create an instance of the builtins
    pBuiltIns = pDevice->getBuiltIns();
    pBuiltIns->setCacheingEnableState(false);

    // set mock compiler to return expected kernel...
    MockCompilerDebugVars fclDebugVars;
    MockCompilerDebugVars igcDebugVars;

    retrieveBinaryKernelFilename(fclDebugVars.fileName, KernelBinaryHelper::BUILT_INS + "_", ".bc");
    retrieveBinaryKernelFilename(igcDebugVars.fileName, KernelBinaryHelper::BUILT_INS + "_", ".gen");

    gEnvironment->fclPushDebugVars(fclDebugVars);
    gEnvironment->igcPushDebugVars(igcDebugVars);
}

void BuiltInFixture::TearDown() {
    gEnvironment->igcPopDebugVars();
    gEnvironment->fclPopDebugVars();
}
