/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/file_io.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/libult/global_environment.h"
#include "shared/test/common/mocks/mock_compiler_interface.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;

class ClCompilerInterfaceTest : public ClDeviceFixture,
                                public ::testing::Test {
  public:
    void SetUp() override {
        ClDeviceFixture::setUp();

        // create the compiler interface
        this->pCompilerInterface = new MockCompilerInterface();
        bool initRet = pCompilerInterface->initialize(std::make_unique<CompilerCache>(CompilerCacheConfig{}), true);
        ASSERT_TRUE(initRet);
        pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->compilerInterface.reset(pCompilerInterface);

        std::string testFile;

        testFile.append(clFiles);
        testFile.append("CopyBuffer_simd16.cl");

        pSource = loadDataFromFile(
            testFile.c_str(),
            sourceSize);

        ASSERT_NE(0u, sourceSize);
        ASSERT_NE(nullptr, pSource);

        inputArgs.src = ArrayRef<char>(pSource.get(), sourceSize);
        inputArgs.internalOptions = ArrayRef<const char>(pClDevice->peekCompilerExtensions().c_str(), pClDevice->peekCompilerExtensions().size());
    }

    void TearDown() override {
        pSource.reset();

        ClDeviceFixture::tearDown();
    }

    MockCompilerInterface *pCompilerInterface;
    TranslationInput inputArgs = {IGC::CodeType::oclC, IGC::CodeType::oclGenBin};
    std::unique_ptr<char[]> pSource = nullptr;
    size_t sourceSize = 0;
};

TEST_F(ClCompilerInterfaceTest, WhenBuildIsInvokedThenFclReceivesListOfExtensionsInInternalOptions) {
    std::string receivedInternalOptions;

    auto debugVars = NEO::getFclDebugVars();
    debugVars.receivedInternalOptionsOutput = &receivedInternalOptions;
    gEnvironment->fclPushDebugVars(debugVars);
    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->build(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::Success, err);
    EXPECT_TRUE(hasSubstr(receivedInternalOptions, pClDevice->peekCompilerExtensions()));
    gEnvironment->fclPopDebugVars();
}

TEST_F(ClCompilerInterfaceTest, WhenCompileIsInvokedThenFclReceivesListOfExtensionsInInternalOptions) {
    std::string receivedInternalOptions;

    MockCompilerDebugVars fclDebugVars;
    retrieveBinaryKernelFilename(fclDebugVars.fileName, "CopyBuffer_simd16_", ".bc");
    fclDebugVars.receivedInternalOptionsOutput = &receivedInternalOptions;
    gEnvironment->fclPushDebugVars(fclDebugVars);
    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->compile(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::Success, err);
    EXPECT_TRUE(hasSubstr(receivedInternalOptions, pClDevice->peekCompilerExtensions()));
    gEnvironment->fclPopDebugVars();
}
