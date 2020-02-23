/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "offline_compiler/offline_compiler.h"
#include "opencl/source/os_interface/os_inc_base.h"
#include "opencl/test/unit_test/helpers/test_files.h"
#include "opencl/test/unit_test/mocks/mock_compilers.h"

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

        NEO::setIgcDebugVars(igcDebugVars);
        NEO::setFclDebugVars(fclDebugVars);
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

    NEO::OsLibrary *libraryFrontEnd;
    NEO::OsLibrary *libraryIGC;

    NEO::MockCompilerDebugVars igcDebugVars;
    NEO::MockCompilerDebugVars fclDebugVars;

    void (*igcSetDebugVarsFPtr)(NEO::MockCompilerDebugVars &debugVars);
    void (*fclSetDebugVarsFPtr)(NEO::MockCompilerDebugVars &debugVars);

    NEO::MockCompilerEnableGuard mockCompilerGuard;

    const std::string devicePrefix;
    const std::string familyNameWithType;
};
