/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "global_environment.h"

#include "core/helpers/hw_info.h"
#include "runtime/os_interface/os_inc_base.h"

TestEnvironment::TestEnvironment(void)
    : libraryFrontEnd(nullptr), libraryIGC(nullptr),
      libraryOS(nullptr), hwInfoDefaultDebugVars() {
    igcDebugVarStack.reserve(3);
    fclDebugVarStack.reserve(3);
}

void TestEnvironment::SetUp() {
    mockCompilerGuard.Enable();

    fclPushDebugVars(fclDefaultDebugVars);
    igcPushDebugVars(igcDefaultDebugVars);
    if (libraryOS == nullptr) {
        libraryOS = setAdapterInfo(&hwInfoDefaultDebugVars.platform,
                                   &hwInfoDefaultDebugVars.gtSystemInfo,
                                   hwInfoDefaultDebugVars.capabilityTable.gpuAddressSpace);
    }
}

void TestEnvironment::TearDown() {
    delete libraryFrontEnd;
    delete libraryIGC;
    if (libraryOS != nullptr) {
        delete libraryOS;
        libraryOS = nullptr;
    }
    mockCompilerGuard.Disable();
}

void TestEnvironment::fclPushDebugVars(
    MockCompilerDebugVars &newDebugVars) {
    fclDebugVarStack.push_back(newDebugVars);
    NEO::setFclDebugVars(newDebugVars);
}

void TestEnvironment::fclPopDebugVars() {
    fclDebugVarStack.pop_back();
    NEO::setFclDebugVars(fclDebugVarStack.back());
}

void TestEnvironment::igcPushDebugVars(
    MockCompilerDebugVars &newDebugVars) {
    igcDebugVarStack.push_back(newDebugVars);
    NEO::setIgcDebugVars(newDebugVars);
}

void TestEnvironment::igcPopDebugVars() {
    igcDebugVarStack.pop_back();
    NEO::setIgcDebugVars(igcDebugVarStack.back());
}

void TestEnvironment::setDefaultDebugVars(
    MockCompilerDebugVars &fclDefaults,
    MockCompilerDebugVars &igcDefaults,
    HardwareInfo &hwInfo) {
    fclDefaultDebugVars = fclDefaults;
    igcDefaultDebugVars = igcDefaults;
    hwInfoDefaultDebugVars = hwInfo;
}

void TestEnvironment::setMockFileNames(
    std::string &fclMockFile,
    std::string &igcMockFile) {
    this->fclMockFile = fclMockFile;
    this->igcMockFile = igcMockFile;
}

std::string &TestEnvironment::fclGetMockFile() {
    return this->fclMockFile;
}

std::string &TestEnvironment::igcGetMockFile() {
    return this->igcMockFile;
}
