/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/fixtures/built_in_fixture.h"

#include "runtime/built_ins/built_ins.h"
#include "runtime/device/device.h"
#include "unit_tests/global_environment.h"
#include "unit_tests/helpers/kernel_binary_helper.h"
#include "unit_tests/helpers/test_files.h"

using namespace NEO;

BuiltInFixture::BuiltInFixture() : pBuiltIns(nullptr) {
}

void BuiltInFixture::SetUp(Device *pDevice) {
    // create an instance of the builtins
    pBuiltIns = pDevice->getExecutionEnvironment()->getBuiltIns();
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
