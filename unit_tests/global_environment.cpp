/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "global_environment.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/os_interface/os_inc_base.h"

TestEnvironment::TestEnvironment(void)
    : libraryFrontEnd(nullptr), libraryIGC(nullptr),
      libraryOS(nullptr) {
    igcDebugVarStack.reserve(3);
    fclDebugVarStack.reserve(3);
}

void TestEnvironment::SetUp() {
    mockCompilerGuard.Enable();

    fclPushDebugVars(fclDefaultDebugVars);
    igcPushDebugVars(igcDefaultDebugVars);
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
    OCLRT::setFclDebugVars(newDebugVars);
}

void TestEnvironment::fclPopDebugVars() {
    fclDebugVarStack.pop_back();
    OCLRT::setFclDebugVars(fclDebugVarStack.back());
}

void TestEnvironment::igcPushDebugVars(
    MockCompilerDebugVars &newDebugVars) {
    igcDebugVarStack.push_back(newDebugVars);
    OCLRT::setIgcDebugVars(newDebugVars);
}

void TestEnvironment::igcPopDebugVars() {
    igcDebugVarStack.pop_back();
    OCLRT::setIgcDebugVars(igcDebugVarStack.back());
}

void TestEnvironment::setDefaultDebugVars(
    MockCompilerDebugVars &fclDefaults,
    MockCompilerDebugVars &igcDefaults,
    HardwareInfo &hwInfo) {
    fclDefaultDebugVars = fclDefaults;
    igcDefaultDebugVars = igcDefaults;

    libraryOS = setAdapterInfo(reinterpret_cast<const void *>(hwInfo.pPlatform), reinterpret_cast<const void *>(hwInfo.pSysInfo));
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
