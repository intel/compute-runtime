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

#pragma once
#include "runtime/helpers/hw_info.h"
#include "runtime/os_interface/os_library.h"
#include "unit_tests/mocks/mock_compilers.h"
#include "gtest/gtest.h"

using namespace OCLRT;

OsLibrary *setAdapterInfo(const PLATFORM *platform, const GT_SYSTEM_INFO *gtSystemInfo);

class TestEnvironment : public ::testing::Environment {
  public:
    TestEnvironment();

    virtual void SetUp() override;

    virtual void TearDown() override;

    virtual void fclPushDebugVars(
        MockCompilerDebugVars &newDebugVars);

    virtual void fclPopDebugVars();

    virtual void igcPushDebugVars(
        MockCompilerDebugVars &newDebugVars);

    virtual void igcPopDebugVars();

    virtual void setDefaultDebugVars(
        MockCompilerDebugVars &fclDefaults,
        MockCompilerDebugVars &igcDefaults,
        HardwareInfo &hwInfo);

    virtual void setMockFileNames(
        std::string &fclMockFile,
        std::string &igcMockFile);

    virtual std::string &fclGetMockFile();
    virtual std::string &igcGetMockFile();

  protected:
    OsLibrary *libraryFrontEnd;
    OsLibrary *libraryIGC;
    OsLibrary *libraryOS;

    std::vector<MockCompilerDebugVars> igcDebugVarStack;
    std::vector<MockCompilerDebugVars> fclDebugVarStack;

    void (*igcSetDebugVarsFPtr)(MockCompilerDebugVars &debugVars);
    void (*fclSetDebugVarsFptr)(MockCompilerDebugVars &debugVars);

    MockCompilerDebugVars fclDefaultDebugVars;
    MockCompilerDebugVars igcDefaultDebugVars;

    std::string fclMockFile;
    std::string igcMockFile;

    MockCompilerEnableGuard mockCompilerGuard;
};

extern TestEnvironment *gEnvironment;
