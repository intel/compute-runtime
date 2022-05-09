/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/offline_compiler/source/offline_compiler.h"
#include "shared/source/os_interface/os_inc_base.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/mocks/mock_compilers.h"

#include "gtest/gtest.h"
#include "mock/mock_iga_dll_guard.h"

class Environment : public ::testing::Environment {
  public:
    Environment(const std::string &devicePrefix, const std::string &familyNameWithType)
        : devicePrefix(devicePrefix), familyNameWithType(familyNameWithType) {
    }

    void SetInputFileName( // NOLINT(readability-identifier-naming)
        const std::string filename) {

        retrieveBinaryKernelFilename(igcDebugVars.fileName, filename + "_", ".gen");
        retrieveBinaryKernelFilename(fclDebugVars.fileName, filename + "_", ".bc");

        NEO::setIgcDebugVars(igcDebugVars);
        NEO::setFclDebugVars(fclDebugVars);
    }

    void SetUp() override {
        mockIgaDllGuard.enable();
        mockCompilerGuard.Enable();
        SetInputFileName("copybuffer");
    }

    void TearDown() override {
        mockCompilerGuard.Disable();
        mockIgaDllGuard.disable();
    }

    NEO::MockCompilerDebugVars igcDebugVars;
    NEO::MockCompilerDebugVars fclDebugVars;

    void (*igcSetDebugVarsFPtr)(NEO::MockCompilerDebugVars &debugVars);
    void (*fclSetDebugVarsFPtr)(NEO::MockCompilerDebugVars &debugVars);

    NEO::MockCompilerEnableGuard mockCompilerGuard;
    NEO::MockIgaDllGuard mockIgaDllGuard;

    const std::string devicePrefix;
    const std::string familyNameWithType;
};
