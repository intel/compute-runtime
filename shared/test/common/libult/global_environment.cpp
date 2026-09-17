/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/global_environment.h"

#include "shared/source/helpers/hw_info.h"

void setAdapterInfo(const PLATFORM *platform, const GT_SYSTEM_INFO *gtSystemInfo, uint64_t gpuAddressSpace);

TestEnvironment *gEnvironment;
TestEnvironment::TestEnvironment() {
    igcDebugVarStack.reserve(3);
    fclDebugVarStack.reserve(3);
}

void TestEnvironment::SetUp() {
    mockCompilerGuard.Enable();

    fclPushDebugVars(fclDefaultDebugVars);
    igcPushDebugVars(igcDefaultDebugVars);
    setupExternalDependencies();
    setAdapterInfo(&hwInfoDefaultDebugVars.platform,
                   &hwInfoDefaultDebugVars.gtSystemInfo,
                   hwInfoDefaultDebugVars.capabilityTable.gpuAddressSpace);
}

void TestEnvironment::TearDown() {
    mockCompilerGuard.Disable();
}

void TestEnvironment::fclPushDebugVars(
    MockCompilerDebugVars &newDebugVars) {
    fclDebugVarStack.push_back(newDebugVars);
    NEO::setFclDebugVars(newDebugVars);
}

void TestEnvironment::fclPopDebugVars() {
    fclDebugVarStack.pop_back();
    if (fclDebugVarStack.empty()) {
        NEO::clearFclDebugVars();
    } else {
        NEO::setFclDebugVars(fclDebugVarStack.back());
    }
}

void TestEnvironment::igcPushDebugVars(
    MockCompilerDebugVars &newDebugVars) {
    igcDebugVarStack.push_back(newDebugVars);
    NEO::setIgcDebugVars(newDebugVars);
}

void TestEnvironment::igcPopDebugVars() {
    igcDebugVarStack.pop_back();
    if (igcDebugVarStack.empty()) {
        NEO::clearIgcDebugVars();
    } else {
        NEO::setIgcDebugVars(igcDebugVarStack.back());
    }
}

void TestEnvironment::setDefaultDebugVars(
    MockCompilerDebugVars &fclDefaults,
    MockCompilerDebugVars &igcDefaults,
    HardwareInfo &hwInfo) {
    fclDefaultDebugVars = fclDefaults;
    igcDefaultDebugVars = igcDefaults;
    hwInfoDefaultDebugVars = hwInfo;
}
