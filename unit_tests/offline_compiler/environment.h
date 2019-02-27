/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "offline_compiler/offline_compiler.h"
#include "runtime/os_interface/os_inc_base.h"
#include "unit_tests/helpers/test_files.h"
#include "unit_tests/mocks/mock_compilers.h"

#include "gtest/gtest.h"

class Environment : public ::testing::Environment {
  public:
    Environment(const std::string &devicePrefix, const std::string &familyNameWithType)
        : libraryFrontEnd(nullptr), libraryIGC(nullptr), devicePrefix(devicePrefix), familyNameWithType(familyNameWithType) {
    }

    void SetInputFileName(
        const std::string filename) {

        retrieveBinaryKernelFilename(igcDebugVars.fileName, filename + "_", ".gen");
        retrieveBinaryKernelFilename(fclDebugVars.fileName, filename + "_", ".bc");

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
    const std::string familyNameWithType;
};
