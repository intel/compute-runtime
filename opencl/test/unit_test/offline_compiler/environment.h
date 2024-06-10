/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/mocks/mock_compilers.h"

#include "gtest/gtest.h"
#include "mock/mock_iga_dll_guard.h"

class Environment : public ::testing::Environment {
  public:
    Environment(const std::string &devicePrefix, const std::string productConfig)
        : devicePrefix(devicePrefix), productConfig(productConfig) {
    }

    void SetInputFileName( // NOLINT(readability-identifier-naming)
        const std::string filename) {

        retrieveBinaryKernelFilename(igcDebugVars.fileName, filename + "_", ".bin");
        retrieveBinaryKernelFilename(fclDebugVars.fileName, filename + "_", ".spv");

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
    const std::string productConfig;
};
