/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/mocks/mock_compilers.h"

#include "gtest/gtest.h"
#include "mock/mock_iga_dll_guard.h"

class Environment : public ::testing::Environment {
  public:
    Environment(const std::string &devicePrefix, const std::string productConfig)
        : devicePrefix(devicePrefix), productConfig(productConfig) {
    }

    void setValidMockKernel() {
        igcDebugVars.binaryToReturn = mockDeviceBinary;
        igcDebugVars.binaryToReturnSize = sizeof(mockDeviceBinary);
        igcDebugVars.debugDataToReturn = mockDeviceBinary;
        igcDebugVars.debugDataToReturnSize = sizeof(mockDeviceBinary);
        fclDebugVars.binaryToReturn = mockDeviceBinary;
        fclDebugVars.binaryToReturnSize = sizeof(mockDeviceBinary);
        fclDebugVars.debugDataToReturn = mockDeviceBinary;
        fclDebugVars.debugDataToReturnSize = sizeof(mockDeviceBinary);

        NEO::setIgcDebugVars(igcDebugVars);
        NEO::setFclDebugVars(fclDebugVars);
    }

    void setInvalidMockKernel() {
        igcDebugVars.binaryToReturn = nullptr;
        igcDebugVars.binaryToReturnSize = 0;
        igcDebugVars.debugDataToReturn = nullptr;
        igcDebugVars.debugDataToReturnSize = 0;
        fclDebugVars.binaryToReturn = nullptr;
        fclDebugVars.binaryToReturnSize = 0;
        fclDebugVars.debugDataToReturn = nullptr;
        fclDebugVars.debugDataToReturnSize = 0;

        NEO::setIgcDebugVars(igcDebugVars);
        NEO::setFclDebugVars(fclDebugVars);
    }

    void SetUp() override {
        mockIgaDllGuard.enable();
        mockCompilerGuard.Enable();
        setValidMockKernel();
    }

    void TearDown() override {
        mockCompilerGuard.Disable();
        mockIgaDllGuard.disable();
    }

    NEO::MockCompilerDebugVars igcDebugVars;
    NEO::MockCompilerDebugVars fclDebugVars;
    char mockDeviceBinary[1] = {8};

    void (*igcSetDebugVarsFPtr)(NEO::MockCompilerDebugVars &debugVars);
    void (*fclSetDebugVarsFPtr)(NEO::MockCompilerDebugVars &debugVars);

    NEO::MockCompilerEnableGuard mockCompilerGuard;
    NEO::MockIgaDllGuard mockIgaDllGuard;

    const std::string devicePrefix;
    const std::string productConfig;
};
