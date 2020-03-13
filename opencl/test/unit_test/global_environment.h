/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/os_library.h"

#include "opencl/test/unit_test/mocks/mock_compilers.h"

#include "gtest/gtest.h"

using namespace NEO;

OsLibrary *setAdapterInfo(const PLATFORM *platform, const GT_SYSTEM_INFO *gtSystemInfo, uint64_t gpuAddressSpace);

class TestEnvironment : public ::testing::Environment {
  public:
    TestEnvironment();

    void SetUp() override;

    void TearDown() override;

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
    OsLibrary *libraryFrontEnd = nullptr;
    OsLibrary *libraryIGC = nullptr;
    OsLibrary *libraryOS = nullptr;

    std::vector<MockCompilerDebugVars> igcDebugVarStack;
    std::vector<MockCompilerDebugVars> fclDebugVarStack;

    void (*igcSetDebugVarsFPtr)(MockCompilerDebugVars &debugVars);
    void (*fclSetDebugVarsFptr)(MockCompilerDebugVars &debugVars);

    MockCompilerDebugVars fclDefaultDebugVars{};
    MockCompilerDebugVars igcDefaultDebugVars{};
    HardwareInfo hwInfoDefaultDebugVars{};

    std::string fclMockFile{};
    std::string igcMockFile{};

    MockCompilerEnableGuard mockCompilerGuard{};
};

extern TestEnvironment *gEnvironment;
