/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_cache.h"
#include "shared/source/helpers/file_io.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/libult/global_environment.h"
#include "shared/test/common/mocks/mock_compiler_interface.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"

using namespace NEO;

class ClCompilerInterfaceTestMockedBinaryFilesTest : public ClDeviceFixture,
                                                     public ::testing::Test {
  public:
    void SetUp() override {
        ClDeviceFixture::setUp();

        // create the compiler interface
        this->pCompilerInterface = new MockCompilerInterface();
        bool initRet = pCompilerInterface->initialize(std::make_unique<CompilerCache>(CompilerCacheConfig{}), true);
        ASSERT_TRUE(initRet);
        pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->compilerInterface.reset(pCompilerInterface);

        inputArgs.src = ArrayRef<const char>(sSource.c_str(), sSource.size() + 1);
        inputArgs.internalOptions = ArrayRef<const char>(pClDevice->peekCompilerExtensions().c_str(), pClDevice->peekCompilerExtensions().size());

        igcDebugVars.binaryToReturn = fakeBinFile;
        igcDebugVars.binaryToReturnSize = sizeof(fakeBinFile);
        igcDebugVars.debugDataToReturn = fakeBinFile;
        igcDebugVars.debugDataToReturnSize = sizeof(fakeBinFile);
        gEnvironment->igcPushDebugVars(igcDebugVars);

        fclDebugVars.binaryToReturn = fakeBinFile;
        fclDebugVars.binaryToReturnSize = sizeof(fakeBinFile);
        fclDebugVars.debugDataToReturn = fakeBinFile;
        fclDebugVars.debugDataToReturnSize = sizeof(fakeBinFile);
        gEnvironment->fclPushDebugVars(fclDebugVars);
    }

    void TearDown() override {
        gEnvironment->fclPopDebugVars();
        gEnvironment->igcPopDebugVars();
        ClDeviceFixture::tearDown();
    }

    MockCompilerInterface *pCompilerInterface;
    TranslationInput inputArgs = {IGC::CodeType::oclC, IGC::CodeType::oclGenBin};
    std::string sSource = "some_kernel(){}";

    DebugManagerStateRestore dbgRestore;
    MockCompilerDebugVars igcDebugVars;
    MockCompilerDebugVars fclDebugVars;
    char fakeBinFile[1] = {8};

    FORBID_REAL_FILE_SYSTEM_CALLS();
};

TEST_F(ClCompilerInterfaceTestMockedBinaryFilesTest, WhenBuildIsInvokedThenFclReceivesListOfExtensionsInInternalOptions) {
    CompilerCacheConfig config = {};
    config.enabled = false;
    auto tempCompilerCache = std::make_unique<CompilerCache>(config);
    pCompilerInterface->cache.reset(tempCompilerCache.release());
    std::string receivedInternalOptions;

    fclDebugVars.receivedInternalOptionsOutput = &receivedInternalOptions;
    gEnvironment->fclPushDebugVars(fclDebugVars);
    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->build(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationErrorCode::success, err);
    EXPECT_TRUE(hasSubstr(receivedInternalOptions, pClDevice->peekCompilerExtensions()));
    gEnvironment->fclPopDebugVars();
}

TEST_F(ClCompilerInterfaceTestMockedBinaryFilesTest, WhenCompileIsInvokedThenFclReceivesListOfExtensionsInInternalOptions) {
    std::string receivedInternalOptions;

    fclDebugVars.receivedInternalOptionsOutput = &receivedInternalOptions;
    gEnvironment->fclPushDebugVars(fclDebugVars);
    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->compile(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationErrorCode::success, err);
    EXPECT_TRUE(hasSubstr(receivedInternalOptions, pClDevice->peekCompilerExtensions()));
    gEnvironment->fclPopDebugVars();
}
