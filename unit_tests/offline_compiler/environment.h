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

#include "runtime/os_interface/os_inc.h"
#include "gtest/gtest.h"
#include "unit_tests/mocks/mock_compilers.h"
#include "unit_tests/helpers/test_files.h"

class Environment : public ::testing::Environment {
  public:
    Environment(const std::string &devicePrefix)
        : libraryFrontEnd(nullptr), libraryIGC(nullptr), devicePrefix(devicePrefix) {
    }

    void SetInputFileName(
        const std::string filename) {

        igcDebugVars.fileName = testFiles + filename + "_" + devicePrefix + ".gen";
        fclDebugVars.fileName = testFiles + filename + "_" + devicePrefix + ".bc";

        OCLRT::setIgcDebugVars(igcDebugVars);
        OCLRT::setFclDebugVars(fclDebugVars);
    }

    void SetUp() override {
        mockCompilerGuard.Enable();
        SetInputFileName("copybuffer");
    }

    void TearDown() override {
        delete libraryFrontEnd;
        delete libraryIGC;
        mockCompilerGuard.Disable();
    }

    OCLRT::OsLibrary *libraryFrontEnd;
    OCLRT::OsLibrary *libraryIGC;

    OCLRT::MockCompilerDebugVars igcDebugVars;
    OCLRT::MockCompilerDebugVars fclDebugVars;

    void (*igcSetDebugVarsFPtr)(OCLRT::MockCompilerDebugVars &debugVars);
    void (*fclSetDebugVarsFPtr)(OCLRT::MockCompilerDebugVars &debugVars);

    OCLRT::MockCompilerEnableGuard mockCompilerGuard;

    const std::string devicePrefix;
};
