/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_cache.h"
#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/compiler_interface/compiler_interface.inl"
#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/source/compiler_interface/oclc_extensions.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/os_inc_base.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/libult/global_environment.h"
#include "shared/test/common/mocks/mock_cif.h"
#include "shared/test/common/mocks/mock_compiler_interface.h"
#include "shared/test/common/mocks/mock_compiler_product_helper.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

#if defined(_WIN32)
const char *gBadDompilerDllName = "bad_compiler.dll";
#elif defined(__linux__)
const char *gCBadDompilerDllName = "libbad_compiler.so";
#else
#error "Unknown OS!"
#endif

class CompilerInterfaceTest : public DeviceFixture,
                              public ::testing::Test {
  public:
    void SetUp() override {
        DeviceFixture::setUp();
        USE_REAL_FILE_SYSTEM();
        // create the compiler interface
        this->pCompilerInterface = new MockCompilerInterface();
        bool initRet = pCompilerInterface->initialize(std::make_unique<CompilerCache>(CompilerCacheConfig{}), true);
        ASSERT_TRUE(initRet);
        pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->compilerInterface.reset(pCompilerInterface);

        std::string testFile;

        testFile.append(clFiles);
        testFile.append("CopyBufferShared_simd32.cl");

        pSource = loadDataFromFile(
            testFile.c_str(),
            sourceSize);

        ASSERT_NE(0u, sourceSize);
        ASSERT_NE(nullptr, pSource);

        inputArgs.src = ArrayRef<char>(pSource.get(), sourceSize);
    }

    void TearDown() override {
        pSource.reset();

        DeviceFixture::tearDown();
    }

    MockCompilerInterface *pCompilerInterface;
    TranslationInput inputArgs = {IGC::CodeType::oclC, IGC::CodeType::oclGenBin};
    std::unique_ptr<char[]> pSource = nullptr;
    size_t sourceSize = 0;
};

TEST(CompilerInterface, WhenInitializeIsCalledThenFailIfCompilerCacheHandlerIsEmpty) {
    MockCompilerInterface ci;

    ci.failLoadFcl = false;
    ci.failLoadIgc = false;
    bool initSuccess = ci.initialize(nullptr, true);
    EXPECT_FALSE(initSuccess);
}

TEST(CompilerInterface, WhenInitializeIsCalledThenFailIfOneOfRequiredCompilersIsUnavailable) {
    bool initSuccess = false;
    bool requireFcl = true;
    MockCompilerInterface ci;

    ci.failLoadFcl = false;
    ci.failLoadIgc = false;
    requireFcl = true;
    initSuccess = ci.initialize(std::make_unique<CompilerCache>(CompilerCacheConfig{}), requireFcl);
    EXPECT_TRUE(initSuccess);

    ci.failLoadFcl = false;
    ci.failLoadIgc = false;
    requireFcl = false;
    initSuccess = ci.initialize(std::make_unique<CompilerCache>(CompilerCacheConfig{}), requireFcl);
    EXPECT_TRUE(initSuccess);

    ci.failLoadFcl = true;
    ci.failLoadIgc = false;
    requireFcl = false;
    initSuccess = ci.initialize(std::make_unique<CompilerCache>(CompilerCacheConfig{}), requireFcl);
    EXPECT_TRUE(initSuccess);

    ci.failLoadFcl = true;
    ci.failLoadIgc = false;
    requireFcl = true;
    initSuccess = ci.initialize(std::make_unique<CompilerCache>(CompilerCacheConfig{}), requireFcl);
    EXPECT_FALSE(initSuccess);

    ci.failLoadFcl = false;
    ci.failLoadIgc = true;
    requireFcl = true;
    initSuccess = ci.initialize(std::make_unique<CompilerCache>(CompilerCacheConfig{}), requireFcl);
    EXPECT_FALSE(initSuccess);

    ci.failLoadFcl = false;
    ci.failLoadIgc = true;
    requireFcl = false;
    initSuccess = ci.initialize(std::make_unique<CompilerCache>(CompilerCacheConfig{}), requireFcl);
    EXPECT_FALSE(initSuccess);

    ci.failLoadFcl = true;
    ci.failLoadIgc = true;
    requireFcl = false;
    initSuccess = ci.initialize(std::make_unique<CompilerCache>(CompilerCacheConfig{}), requireFcl);
    EXPECT_FALSE(initSuccess);

    ci.failLoadFcl = true;
    ci.failLoadIgc = true;
    requireFcl = true;
    initSuccess = ci.initialize(std::make_unique<CompilerCache>(CompilerCacheConfig{}), requireFcl);
    EXPECT_FALSE(initSuccess);
}

TEST(CompilerInterfaceCreateInstance, WhenInitializeFailedThenReturnNull) {
    struct FailInitializeCompilerInterface : CompilerInterface {
        bool initialize(std::unique_ptr<CompilerCache> &&cache, bool requireFcl) override {
            return false;
        }
    };
    EXPECT_EQ(nullptr, CompilerInterface::createInstance<FailInitializeCompilerInterface>(std::make_unique<CompilerCache>(CompilerCacheConfig{}), false));
}

TEST_F(CompilerInterfaceTest, WhenCompilingToIsaThenSuccessIsReturned) {
    USE_REAL_FILE_SYSTEM();

    TranslationOutput translationOutput;
    auto err = pCompilerInterface->build(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::success, err);
}

TEST_F(CompilerInterfaceTest, WhenPreferredIntermediateRepresentationSpecifiedThenPreserveIt) {
    USE_REAL_FILE_SYSTEM();

    CompilerCacheConfig config = {};
    config.enabled = false;
    auto tempCompilerCache = std::make_unique<CompilerCache>(config);
    pCompilerInterface->cache.reset(tempCompilerCache.release());
    TranslationOutput translationOutput;
    inputArgs.preferredIntermediateType = IGC::CodeType::llvmLl;
    auto err = pCompilerInterface->build(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(IGC::CodeType::llvmLl, translationOutput.intermediateCodeType);
    EXPECT_EQ(TranslationOutput::ErrorCode::success, err);
}

TEST_F(CompilerInterfaceTest, whenCompilerIsNotAvailableThenBuildFailsGracefully) {
    pCompilerInterface->defaultIgc.entryPoint.reset(nullptr);
    pCompilerInterface->failLoadIgc = true;

    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->build(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::compilerNotAvailable, err);
}

TEST_F(CompilerInterfaceTest, whenFclTranslatorReturnsNullptrThenBuildFailsGracefully) {
    CompilerCacheConfig config = {};
    config.enabled = false;
    auto tempCompilerCache = std::make_unique<CompilerCache>(config);
    pCompilerInterface->cache.reset(tempCompilerCache.release());
    pCompilerInterface->failCreateFclTranslationCtx = true;
    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->build(*pDevice, inputArgs, translationOutput);
    pCompilerInterface->failCreateFclTranslationCtx = false;
    EXPECT_EQ(TranslationOutput::ErrorCode::unknownError, err);
}

TEST_F(CompilerInterfaceTest, whenIgcTranslatorReturnsNullptrThenBuildFailsGracefully) {
    USE_REAL_FILE_SYSTEM();

    CompilerCacheConfig config = {};
    config.enabled = false;
    auto tempCompilerCache = std::make_unique<CompilerCache>(config);
    pCompilerInterface->cache.reset(tempCompilerCache.release());
    pCompilerInterface->failCreateIgcTranslationCtx = true;
    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->build(*pDevice, inputArgs, translationOutput);
    pCompilerInterface->failCreateIgcTranslationCtx = true;
    EXPECT_EQ(TranslationOutput::ErrorCode::unknownError, err);
}

TEST_F(CompilerInterfaceTest, GivenOptionsWhenCompilingToIsaThenSuccessIsReturned) {
    USE_REAL_FILE_SYSTEM();

    std::string internalOptions = "SOME_OPTION";

    MockCompilerDebugVars fclDebugVars;
    fclDebugVars.fileName = gEnvironment->fclGetMockFile();
    fclDebugVars.internalOptionsExpected = true;
    gEnvironment->fclPushDebugVars(fclDebugVars);

    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.fileName = gEnvironment->igcGetMockFile();
    igcDebugVars.internalOptionsExpected = true;
    gEnvironment->igcPushDebugVars(igcDebugVars);

    inputArgs.internalOptions = ArrayRef<const char>(internalOptions.c_str(), internalOptions.length());

    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->build(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::success, err);

    gEnvironment->fclPopDebugVars();
    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceTest, WhenCompilingToIrThenSuccessIsReturned) {
    USE_REAL_FILE_SYSTEM();

    MockCompilerDebugVars fclDebugVars;
    retrieveBinaryKernelFilename(fclDebugVars.fileName, "CopyBufferShared_simd32_", ".spv");
    gEnvironment->fclPushDebugVars(fclDebugVars);
    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->compile(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::success, err);

    gEnvironment->fclPopDebugVars();
}

TEST_F(CompilerInterfaceTest, GivenFclRedirectionEnvSetToForceIgcWhenCompilingToIrThenIgcIsBeingUsed) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.UseIgcAsFcl.set(1);

    char bin[1] = {7};
    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.binaryToReturn = bin;
    igcDebugVars.binaryToReturnSize = 1;
    gEnvironment->igcPushDebugVars(igcDebugVars);

    MockCompilerDebugVars fclDebugVars;
    fclDebugVars.forceBuildFailure = true;
    gEnvironment->fclPushDebugVars(fclDebugVars);

    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->compile(*pDevice, inputArgs, translationOutput);
    gEnvironment->fclPopDebugVars();
    gEnvironment->igcPopDebugVars();
    EXPECT_EQ(TranslationOutput::ErrorCode::success, err);
    ASSERT_EQ(1U, translationOutput.intermediateRepresentation.size);
    EXPECT_EQ(7, translationOutput.intermediateRepresentation.mem[0]);
}

TEST_F(CompilerInterfaceTest, GivenFclRedirectionEnvSetToForceFclWhenCompilingToIrThenFclIsBeingUsed) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.UseIgcAsFcl.set(2);

    char bin[1] = {7};
    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.forceBuildFailure = true;
    gEnvironment->igcPushDebugVars(igcDebugVars);

    MockCompilerDebugVars fclDebugVars;
    fclDebugVars.binaryToReturn = bin;
    fclDebugVars.binaryToReturnSize = 1;
    gEnvironment->fclPushDebugVars(fclDebugVars);

    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->compile(*pDevice, inputArgs, translationOutput);
    gEnvironment->fclPopDebugVars();
    gEnvironment->igcPopDebugVars();
    EXPECT_EQ(TranslationOutput::ErrorCode::success, err);
    ASSERT_EQ(1U, translationOutput.intermediateRepresentation.size);
    EXPECT_EQ(7, translationOutput.intermediateRepresentation.mem[0]);
}

TEST_F(CompilerInterfaceTest, GivenFclRedirectionDefaultSettingWhenCompilingToIrThenUseCompilerProductHelperDefaults) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.UseIgcAsFcl.set(0);

    EXPECT_EQ(this->pDevice->getCompilerProductHelper().useIgcAsFcl(), pCompilerInterface->useIgcAsFcl(this->pDevice));
}

TEST_F(CompilerInterfaceTest, GivenFclRedirectionEnvSetToForceIgcWhenBuildingDeviceBinaryThenOnlyIgcIsBeingUsed) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.UseIgcAsFcl.set(1);

    char bin[1] = {7};
    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.binaryToReturn = bin;
    igcDebugVars.binaryToReturnSize = 1;
    gEnvironment->igcPushDebugVars(igcDebugVars);

    MockCompilerDebugVars fclDebugVars;
    fclDebugVars.forceBuildFailure = true;
    gEnvironment->fclPushDebugVars(fclDebugVars);

    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->build(*pDevice, inputArgs, translationOutput);
    gEnvironment->fclPopDebugVars();
    gEnvironment->igcPopDebugVars();
    EXPECT_EQ(TranslationOutput::ErrorCode::success, err);
    ASSERT_EQ(1U, translationOutput.intermediateRepresentation.size);
    EXPECT_EQ(7, translationOutput.intermediateRepresentation.mem[0]);
}

TEST_F(CompilerInterfaceTest, GivenFclRedirectionEnvSetToForceFclWhenBuildingDeviceBinaryThenFclIsBeingUsedForSourceTranslation) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.UseIgcAsFcl.set(2);

    char fclBin[2] = {3, 5};
    char igcBin[1] = {7};
    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.binaryToReturn = igcBin;
    igcDebugVars.binaryToReturnSize = 1;
    gEnvironment->igcPushDebugVars(igcDebugVars);

    MockCompilerDebugVars fclDebugVars;
    fclDebugVars.binaryToReturn = fclBin;
    fclDebugVars.binaryToReturnSize = 2;
    gEnvironment->fclPushDebugVars(fclDebugVars);

    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->build(*pDevice, inputArgs, translationOutput);
    gEnvironment->fclPopDebugVars();
    gEnvironment->igcPopDebugVars();
    EXPECT_EQ(TranslationOutput::ErrorCode::success, err);
    ASSERT_EQ(2U, translationOutput.intermediateRepresentation.size);
    EXPECT_EQ(3, translationOutput.intermediateRepresentation.mem[0]);
    EXPECT_EQ(5, translationOutput.intermediateRepresentation.mem[1]);
}

TEST_F(CompilerInterfaceTest, GivenProgramCreatedFromIrWhenCompileIsCalledThenDontRecompile) {
    TranslationOutput translationOutput = {};
    inputArgs.srcType = IGC::CodeType::spirV;
    auto err = pCompilerInterface->compile(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::alreadyCompiled, err);

    inputArgs.srcType = IGC::CodeType::llvmBc;
    err = pCompilerInterface->compile(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::alreadyCompiled, err);

    inputArgs.srcType = IGC::CodeType::llvmLl;
    err = pCompilerInterface->compile(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::alreadyCompiled, err);

    inputArgs.srcType = IGC::CodeType::oclGenBin;
    err = pCompilerInterface->compile(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::alreadyCompiled, err);
}

TEST_F(CompilerInterfaceTest, whenCompilerIsNotAvailableThenCompileFailsGracefully) {
    MockCompilerDebugVars fclDebugVars;
    fclDebugVars.fileName = clFiles + "copybuffer.elf";
    gEnvironment->fclPushDebugVars(fclDebugVars);
    pCompilerInterface->defaultIgc.entryPoint->Release();
    pCompilerInterface->setIgcMain(nullptr);
    pCompilerInterface->failLoadIgc = true;
    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->compile(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::compilerNotAvailable, err);

    gEnvironment->fclPopDebugVars();
}

TEST_F(CompilerInterfaceTest, whenFclTranslatorReturnsNullptrThenCompileFailsGracefully) {
    MockCompilerDebugVars fclDebugVars;
    fclDebugVars.fileName = clFiles + "copybuffer.elf";
    gEnvironment->fclPushDebugVars(fclDebugVars);
    pCompilerInterface->failCreateFclTranslationCtx = true;
    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->compile(*pDevice, inputArgs, translationOutput);
    pCompilerInterface->failCreateFclTranslationCtx = false;
    EXPECT_EQ(TranslationOutput::ErrorCode::unknownError, err);

    gEnvironment->fclPopDebugVars();
}

TEST_F(CompilerInterfaceTest, GivenForceBuildFailureWhenCompilingToIrThenCompilationFailureErrorIsReturned) {
    MockCompilerDebugVars fclDebugVars;
    fclDebugVars.fileName = "../copybuffer.elf";
    fclDebugVars.forceBuildFailure = true;
    gEnvironment->fclPushDebugVars(fclDebugVars);
    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->compile(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::compilationFailure, err);

    gEnvironment->fclPopDebugVars();
}

TEST_F(CompilerInterfaceTest, GivenForceBuildFailureWhenLinkingIrThenLinkFailureErrorIsReturned) {
    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.fileName = "../copybuffer.ll";
    igcDebugVars.forceBuildFailure = true;
    gEnvironment->igcPushDebugVars(igcDebugVars);
    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->link(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::linkFailure, err);

    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceTest, WhenLinkIsCalledThenOclGenBinIsTheTranslationTarget) {
    USE_REAL_FILE_SYSTEM();

    // link only from .ll to gen ISA
    MockCompilerDebugVars igcDebugVars;
    retrieveBinaryKernelFilename(igcDebugVars.fileName, "CopyBufferShared_simd32_", ".spv");
    gEnvironment->igcPushDebugVars(igcDebugVars);
    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->link(*pDevice, inputArgs, translationOutput);
    gEnvironment->igcPopDebugVars();
    ASSERT_EQ(TranslationOutput::ErrorCode::success, err);
    ASSERT_EQ(1u, pCompilerInterface->requestedTranslationCtxs.size());

    MockCompilerInterface::TranslationOpT translation = {IGC::CodeType::elf, IGC::CodeType::oclGenBin};
    EXPECT_EQ(translation, pCompilerInterface->requestedTranslationCtxs[0]);
}

TEST_F(CompilerInterfaceTest, whenCompilerIsNotAvailableThenLinkFailsGracefully) {
    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.fileName = clFiles + "copybuffer.ll";
    gEnvironment->igcPushDebugVars(igcDebugVars);
    pCompilerInterface->defaultIgc.entryPoint->Release();
    pCompilerInterface->setIgcMain(nullptr);
    pCompilerInterface->failLoadIgc = true;
    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->link(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::compilerNotAvailable, err);

    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceTest, whenSrcAllocationFailsThenLinkFailsGracefully) {
    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.fileName = clFiles + "copybuffer.ll";
    gEnvironment->igcPushDebugVars(igcDebugVars);
    MockCIFBuffer::failAllocations = true;
    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->link(*pDevice, inputArgs, translationOutput);
    MockCIFBuffer::failAllocations = false;
    EXPECT_EQ(TranslationOutput::ErrorCode::unknownError, err);

    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceTest, whenTranslateReturnsNullptrThenLinkFailsGracefully) {
    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.fileName = clFiles + "copybuffer.ll";
    gEnvironment->igcPushDebugVars(igcDebugVars);
    pCompilerInterface->failCreateIgcTranslationCtx = true;
    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->link(*pDevice, inputArgs, translationOutput);
    pCompilerInterface->failCreateIgcTranslationCtx = false;
    EXPECT_EQ(TranslationOutput::ErrorCode::unknownError, err);

    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceTest, GivenForceBuildFailureWhenCreatingLibraryThenLinkFailureErrorIsReturned) {
    // create library from .ll to IR
    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.fileName = "../copybuffer.ll";
    igcDebugVars.forceBuildFailure = true;
    gEnvironment->igcPushDebugVars(igcDebugVars);
    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->createLibrary(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::linkFailure, err);

    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceTest, WhenCreateLibraryIsCalledThenLlvmBcIsUsedAsIntermediateRepresentation) {
    USE_REAL_FILE_SYSTEM();

    // create library from .ll to IR
    MockCompilerDebugVars igcDebugVars;
    retrieveBinaryKernelFilename(igcDebugVars.fileName, "CopyBufferShared_simd32_", ".spv");
    gEnvironment->igcPushDebugVars(igcDebugVars);
    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->createLibrary(*pDevice, inputArgs, translationOutput);
    gEnvironment->igcPopDebugVars();
    EXPECT_EQ(TranslationOutput::ErrorCode::success, err);
    ASSERT_EQ(1U, pCompilerInterface->requestedTranslationCtxs.size());

    EXPECT_EQ(IGC::CodeType::llvmBc, pCompilerInterface->requestedTranslationCtxs[0].second);
}

TEST_F(CompilerInterfaceTest, whenCompilerIsNotAvailableThenCreateLibraryFailsGracefully) {
    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.fileName = clFiles + "copybuffer.ll";
    gEnvironment->igcPushDebugVars(igcDebugVars);
    pCompilerInterface->defaultIgc.entryPoint->Release();
    pCompilerInterface->setIgcMain(nullptr);
    pCompilerInterface->failLoadIgc = true;

    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->createLibrary(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::compilerNotAvailable, err);

    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceTest, whenIgcTranslatorReturnsNullptrThenCreateLibraryFailsGracefully) {
    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.fileName = clFiles + "copybuffer.ll";
    gEnvironment->igcPushDebugVars(igcDebugVars);
    pCompilerInterface->failCreateIgcTranslationCtx = true;
    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->createLibrary(*pDevice, inputArgs, translationOutput);
    pCompilerInterface->failCreateIgcTranslationCtx = false;
    EXPECT_EQ(TranslationOutput::ErrorCode::unknownError, err);

    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceTest, GivenForceBuildFailureWhenFclBuildingThenBuildFailureErrorIsReturned) {
    CompilerCacheConfig config = {};
    config.enabled = false;
    auto tempCompilerCache = std::make_unique<CompilerCache>(config);
    pCompilerInterface->cache.reset(tempCompilerCache.release());
    MockCompilerDebugVars fclDebugVars;
    fclDebugVars.forceCreateFailure = false;
    fclDebugVars.forceBuildFailure = true;
    fclDebugVars.forceRegisterFail = false;
    fclDebugVars.fileName = "copybuffer_skl.spv";

    gEnvironment->fclPushDebugVars(fclDebugVars);

    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->build(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::buildFailure, err);

    gEnvironment->fclPopDebugVars();
}

TEST_F(CompilerInterfaceTest, GivenForceBuildFailureWhenIgcBuildingThenBuildFailureErrorIsReturned) {
    CompilerCacheConfig config = {};
    config.enabled = false;
    auto tempCompilerCache = std::make_unique<CompilerCache>(config);
    pCompilerInterface->cache.reset(tempCompilerCache.release());
    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.forceCreateFailure = false;
    igcDebugVars.forceBuildFailure = true;
    igcDebugVars.forceRegisterFail = false;
    igcDebugVars.fileName = "copybuffer_skl.gen";

    gEnvironment->igcPushDebugVars(igcDebugVars);

    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->build(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::buildFailure, err);

    gEnvironment->igcPopDebugVars();
}

struct TranslationCtxMock {
    bool returnNullptr = false;
    bool returnNullptrOutput = false;
    bool returnNullptrLog = false;
    bool returnNullptrDebugData = false;

    CIF::Builtins::BufferSimple *receivedSrc = nullptr;
    CIF::Builtins::BufferSimple *receivedOpt = nullptr;
    CIF::Builtins::BufferSimple *receivedIntOpt = nullptr;
    CIF::Builtins::BufferSimple *receivedTracingOpt = nullptr;

    CIF::RAII::UPtr_t<IGC::OclTranslationOutputTagOCL> Translate(CIF::Builtins::BufferSimple *src, // NOLINT(readability-identifier-naming)
                                                                 CIF::Builtins::BufferSimple *options,
                                                                 CIF::Builtins::BufferSimple *internalOptions,
                                                                 CIF::Builtins::BufferSimple *tracingOptions,
                                                                 uint32_t tracingOptionsCount) {
        this->receivedSrc = src;
        this->receivedOpt = options;
        this->receivedIntOpt = internalOptions;
        this->receivedTracingOpt = tracingOptions;

        if (returnNullptr) {
            return CIF::RAII::UPtr_t<IGC::OclTranslationOutputTagOCL>(nullptr);
        }

        auto ret = new MockOclTranslationOutput();
        if (returnNullptrOutput) {
            ret->output->Release();
            ret->output = nullptr;
        }

        if (returnNullptrLog) {
            ret->log->Release();
            ret->log = nullptr;
        }

        if (returnNullptrDebugData) {
            ret->debugData->Release();
            ret->debugData = nullptr;
        }

        return CIF::RAII::UPtr_t<IGC::OclTranslationOutputTagOCL>(ret);
    }
    CIF::RAII::UPtr_t<IGC::OclTranslationOutputTagOCL> Translate(CIF::Builtins::BufferSimple *src, // NOLINT(readability-identifier-naming)
                                                                 CIF::Builtins::BufferSimple *options,
                                                                 CIF::Builtins::BufferSimple *internalOptions,
                                                                 CIF::Builtins::BufferSimple *tracingOptions,
                                                                 uint32_t tracingOptionsCount,
                                                                 void *gtpinInit) {
        return this->Translate(src, options, internalOptions, tracingOptions, tracingOptionsCount);
    }
    CIF::RAII::UPtr_t<IGC::OclTranslationOutputTagOCL> Translate(CIF::Builtins::BufferSimple *src, // NOLINT(readability-identifier-naming)
                                                                 CIF::Builtins::BufferSimple *specConstantsIds,
                                                                 CIF::Builtins::BufferSimple *specConstantsValues,
                                                                 CIF::Builtins::BufferSimple *options,
                                                                 CIF::Builtins::BufferSimple *internalOptions,
                                                                 CIF::Builtins::BufferSimple *tracingOptions,
                                                                 uint32_t tracingOptionsCount,
                                                                 void *gtPinInput) {
        return this->Translate(src, options, internalOptions, tracingOptions, tracingOptionsCount);
    }
};

TEST(TranslateTest, whenArgsAreValidAndTranslatorReturnsValidOutputThenValidOutputIsReturned) {
    TranslationCtxMock mockTranslationCtx;
    auto mockSrc = std::make_unique<MockCIFBuffer>();
    auto mockOpt = std::make_unique<MockCIFBuffer>();
    auto mockIntOpt = std::make_unique<MockCIFBuffer>();

    auto ret = NEO::translate(&mockTranslationCtx, mockSrc.get(), mockOpt.get(), mockIntOpt.get());
    EXPECT_NE(nullptr, ret);

    EXPECT_EQ(mockSrc.get(), mockTranslationCtx.receivedSrc);
    EXPECT_EQ(mockOpt.get(), mockTranslationCtx.receivedOpt);
    EXPECT_EQ(mockIntOpt.get(), mockTranslationCtx.receivedIntOpt);
}

TEST(TranslateTest, givenGtPinInputWhenArgsAreValidAndTranslatorReturnsValidOutputThenValidOutputIsReturned) {
    TranslationCtxMock mockTranslationCtx;
    auto mockSrc = std::make_unique<MockCIFBuffer>();
    auto mockOpt = std::make_unique<MockCIFBuffer>();
    auto mockIntOpt = std::make_unique<MockCIFBuffer>();

    auto ret = NEO::translate(&mockTranslationCtx, mockSrc.get(), mockOpt.get(), mockIntOpt.get(), nullptr);
    EXPECT_NE(nullptr, ret);

    EXPECT_EQ(mockSrc.get(), mockTranslationCtx.receivedSrc);
    EXPECT_EQ(mockOpt.get(), mockTranslationCtx.receivedOpt);
    EXPECT_EQ(mockIntOpt.get(), mockTranslationCtx.receivedIntOpt);
}

TEST(TranslateTest, whenArgsAreInvalidThenNullptrIsReturned) {
    auto mockSrc = std::make_unique<MockCIFBuffer>();
    auto mockOpt = std::make_unique<MockCIFBuffer>();
    auto mockIntOpt = std::make_unique<MockCIFBuffer>();

    auto ret = NEO::translate<TranslationCtxMock>(nullptr, mockSrc.get(), mockOpt.get(), mockIntOpt.get());

    EXPECT_EQ(nullptr, ret);
}

TEST(TranslateTest, givenGtPinInputWhenArgsAreInvalidThenNullptrIsReturned) {
    auto mockSrc = std::make_unique<MockCIFBuffer>();
    auto mockOpt = std::make_unique<MockCIFBuffer>();
    auto mockIntOpt = std::make_unique<MockCIFBuffer>();

    auto ret = NEO::translate<TranslationCtxMock>(nullptr, mockSrc.get(), mockOpt.get(), mockIntOpt.get(), nullptr);

    EXPECT_EQ(nullptr, ret);
}

TEST(TranslateTest, whenTranslatorReturnsNullptrThenNullptrIsReturned) {
    TranslationCtxMock mockTranslationCtx;
    mockTranslationCtx.returnNullptr = true;
    auto mockCifBuffer = std::make_unique<MockCIFBuffer>();

    auto ret = NEO::translate(&mockTranslationCtx, mockCifBuffer.get(), mockCifBuffer.get(), mockCifBuffer.get());
    EXPECT_EQ(nullptr, ret);
}

TEST(TranslateTest, givenSpecConstantsBuffersWhenTranslatorReturnsNullptrThenNullptrIsReturned) {
    TranslationCtxMock mockTranslationCtx;
    mockTranslationCtx.returnNullptr = true;
    auto mockCifBuffer = std::make_unique<MockCIFBuffer>();

    auto ret = NEO::translate(&mockTranslationCtx, mockCifBuffer.get(), mockCifBuffer.get(), mockCifBuffer.get(), mockCifBuffer.get(), mockCifBuffer.get(), nullptr);
    EXPECT_EQ(nullptr, ret);
}

TEST(TranslateTest, givenNullPtrAsGtPinInputWhenTranslatorReturnsNullptrThenNullptrIsReturned) {
    TranslationCtxMock mockTranslationCtx;
    mockTranslationCtx.returnNullptr = true;
    auto mockCifBuffer = std::make_unique<MockCIFBuffer>();

    auto ret = NEO::translate(&mockTranslationCtx, mockCifBuffer.get(), mockCifBuffer.get(), mockCifBuffer.get(), nullptr);
    EXPECT_EQ(nullptr, ret);
}

TEST(TranslateTest, whenTranslatorReturnsInvalidOutputThenNullptrIsReturned) {
    TranslationCtxMock mockTranslationCtx;
    auto mockCifBuffer = std::make_unique<MockCIFBuffer>();
    for (uint32_t i = 1; i <= maxNBitValue(3); ++i) {
        mockTranslationCtx.returnNullptrDebugData = (i & 1) != 0;
        mockTranslationCtx.returnNullptrLog = (i & (1 << 1)) != 0;
        mockTranslationCtx.returnNullptrOutput = (i & (1 << 2)) != 0;
        auto ret = NEO::translate(&mockTranslationCtx, mockCifBuffer.get(), mockCifBuffer.get(), mockCifBuffer.get());
        EXPECT_EQ(nullptr, ret);
    }
}

TEST(TranslateTest, givenNullPtrAsGtPinInputWhenTranslatorReturnsInvalidOutputThenNullptrIsReturned) {
    TranslationCtxMock mockTranslationCtx;
    auto mockCifBuffer = std::make_unique<MockCIFBuffer>();
    for (uint32_t i = 1; i <= maxNBitValue(3); ++i) {
        mockTranslationCtx.returnNullptrDebugData = (i & 1) != 0;
        mockTranslationCtx.returnNullptrLog = (i & (1 << 1)) != 0;
        mockTranslationCtx.returnNullptrOutput = (i & (1 << 2)) != 0;
        auto ret = NEO::translate(&mockTranslationCtx, mockCifBuffer.get(), mockCifBuffer.get(), mockCifBuffer.get(), nullptr);
        EXPECT_EQ(nullptr, ret);
    }
}

TEST(TranslateTest, givenSpecConstantsBuffersAndNullPtrAsGtPinInputWhenTranslatorReturnsInvalidOutputThenNullptrIsReturned) {
    TranslationCtxMock mockTranslationCtx;
    auto mockCifBuffer = std::make_unique<MockCIFBuffer>();
    for (uint32_t i = 1; i <= maxNBitValue(3); ++i) {
        mockTranslationCtx.returnNullptrDebugData = (i & 1) != 0;
        mockTranslationCtx.returnNullptrLog = (i & (1 << 1)) != 0;
        mockTranslationCtx.returnNullptrOutput = (i & (1 << 2)) != 0;
        auto ret = NEO::translate(&mockTranslationCtx, mockCifBuffer.get(), mockCifBuffer.get(), mockCifBuffer.get(), mockCifBuffer.get(), mockCifBuffer.get(), nullptr);
        EXPECT_EQ(nullptr, ret);
    }
}

TEST(TranslateTest, whenAnyArgIsNullThenNullptrIsReturnedAndTranslatorIsNotInvoked) {
    TranslationCtxMock mockTranslationCtx;
    auto mockCifBuffer = std::make_unique<MockCIFBuffer>();
    for (uint32_t i = 0; i < maxNBitValue(3); ++i) {
        auto src = (i & 1) ? mockCifBuffer.get() : nullptr;
        auto opts = (i & (1 << 1)) ? mockCifBuffer.get() : nullptr;
        auto intOpts = (i & (1 << 2)) ? mockCifBuffer.get() : nullptr;

        auto ret = NEO::translate(&mockTranslationCtx, src, opts, intOpts);
        EXPECT_EQ(nullptr, ret);
    }

    EXPECT_EQ(nullptr, mockTranslationCtx.receivedSrc);
    EXPECT_EQ(nullptr, mockTranslationCtx.receivedOpt);
    EXPECT_EQ(nullptr, mockTranslationCtx.receivedIntOpt);
    EXPECT_EQ(nullptr, mockTranslationCtx.receivedTracingOpt);
}

TEST(LoadCompilerTest, whenEverythingIsOkThenReturnsTrueAndValidOutputs) {
    std::unique_ptr<NEO::OsLibrary> retLib;
    CIF::RAII::UPtr_t<CIF::CIFMain> retMain;
    bool retVal = loadCompiler<IGC::IgcOclDeviceCtx>("", retLib, retMain);
    EXPECT_TRUE(retVal);
    EXPECT_NE(nullptr, retLib.get());
    EXPECT_NE(nullptr, retMain.get());
}

TEST(LoadCompilerTest, whenCouldNotLoadLibraryThenReturnFalseAndNullOutputs) {
    std::unique_ptr<NEO::OsLibrary> retLib;
    CIF::RAII::UPtr_t<CIF::CIFMain> retMain;
    bool retVal = loadCompiler<IGC::IgcOclDeviceCtx>("_falseName.notRealLib", retLib, retMain);
    EXPECT_FALSE(retVal);
    EXPECT_EQ(nullptr, retLib.get());
    EXPECT_EQ(nullptr, retMain.get());
}

TEST(LoadCompilerTest, whenCreateMainFailsThenReturnFalseAndNullOutputs) {
    NEO::failCreateCifMain = true;

    std::unique_ptr<NEO::OsLibrary> retLib;
    CIF::RAII::UPtr_t<CIF::CIFMain> retMain;
    bool retVal = loadCompiler<IGC::IgcOclDeviceCtx>("", retLib, retMain);
    EXPECT_FALSE(retVal);
    EXPECT_EQ(nullptr, retLib.get());
    EXPECT_EQ(nullptr, retMain.get());

    NEO::failCreateCifMain = false;
}

TEST(LoadCompilerTest, whenEntrypointInterfaceIsNotCompatibleThenReturnFalseAndNullOutputs) {

    std::unique_ptr<NEO::OsLibrary> retLib;
    CIF::RAII::UPtr_t<CIF::CIFMain> retMain;
    bool retVal = loadCompiler<IGC::GTSystemInfo>("", retLib, retMain);
    EXPECT_FALSE(retVal);
    EXPECT_EQ(nullptr, retLib.get());
    EXPECT_EQ(nullptr, retMain.get());
}

TEST(LoadCompilerTest, whenLoadCompilerWhenEntryIgcOclDeviceCtxThenIgnoreIgcsIcbeVersion) {
    std::unique_ptr<NEO::OsLibrary> retLib;
    CIF::RAII::UPtr_t<CIF::CIFMain> retMain;
    bool retVal = loadCompiler<IGC::IgcOclDeviceCtx>("", retLib, retMain);
    EXPECT_TRUE(retVal);
    EXPECT_NE(nullptr, retLib.get());
    EXPECT_NE(nullptr, retMain.get());
}

template <typename DeviceCtxBase, typename TranslationCtx>
struct MockCompilerDeviceCtx : DeviceCtxBase {
    TranslationCtx *CreateTranslationCtxImpl(CIF::Version_t ver, IGC::CodeType::CodeType_t inType,
                                             IGC::CodeType::CodeType_t outType) override {
        returned = new TranslationCtx;
        return returned;
    }

    TranslationCtx *returned = nullptr;
};

template <typename DeviceCtx, typename MockDeviceCtx>
struct LockListener {
    LockListener(NEO::Device *device, bool createDeviceCtxOnLock = true)
        : device(device), createDeviceCtxOnLock(createDeviceCtxOnLock) {
    }

    static void listener(MockCompilerInterface &compInt) {
        auto data = (LockListener *)compInt.lockListenerData;
        data->lockCount += 1;
        if (data->createDeviceCtxOnLock && compInt.getDeviceContexts<DeviceCtx>().empty()) {

            auto deviceCtx = CIF::RAII::UPtr(new MockDeviceCtx);
            compInt.setDeviceCtx(*data->device, deviceCtx.get());
            data->createdDeviceCtx = deviceCtx.get();
        }
    }

    NEO::Device *device = nullptr;
    MockDeviceCtx *createdDeviceCtx = nullptr;
    bool createDeviceCtxOnLock = false;
    int lockCount = 0;
};

struct WasLockedListener {
    static void listener(MockCompilerInterface &compInt) {
        auto data = (WasLockedListener *)compInt.lockListenerData;
        data->wasLocked = true;
    }
    bool wasLocked = false;
};

TEST_F(CompilerInterfaceTest, givenUpdatedSpecConstValuesWhenBuildProgramThenProperValuesArePassed) {
    USE_REAL_FILE_SYSTEM();

    struct MockTranslationContextSpecConst : public MockIgcOclTranslationCtx {
        IGC::OclTranslationOutputBase *TranslateImpl(
            CIF::Version_t outVersion,
            CIF::Builtins::BufferSimple *src,
            CIF::Builtins::BufferSimple *specConstantsIds,
            CIF::Builtins::BufferSimple *specConstantsValues,
            CIF::Builtins::BufferSimple *options,
            CIF::Builtins::BufferSimple *internalOptions,
            CIF::Builtins::BufferSimple *tracingOptions,
            uint32_t tracingOptionsCount,
            void *gtPinInput) override {
            EXPECT_EQ(10u, specConstantsIds->GetMemory<uint32_t>()[0]);
            EXPECT_EQ(100u, specConstantsValues->GetMemory<uint64_t>()[0]);
            return new MockOclTranslationOutput();
        }
    };

    auto specConstCtx = CIF::RAII::UPtr(new MockCompilerDeviceCtx<MockIgcOclDeviceCtx, MockTranslationContextSpecConst>());
    pCompilerInterface->setDeviceCtx(*pDevice, specConstCtx.get());

    specConstValuesMap specConst{{10, 100}};
    inputArgs.specializedValues = specConst;

    TranslationOutput translationOutput;
    auto err = pCompilerInterface->build(*pDevice, inputArgs, translationOutput);

    EXPECT_EQ(TranslationOutput::ErrorCode::success, err);
} // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks), NEO-14033

TEST_F(CompilerInterfaceTest, GivenRequestForNewFclTranslationCtxWhenDeviceCtxIsNotAvailableThenCreateNewDeviceCtxAndUseItToReturnValidTranslationCtx) {
    auto device = this->pDevice;
    auto ret = this->pCompilerInterface->createFclTranslationCtx(*device, IGC::CodeType::oclC, IGC::CodeType::spirV);
    EXPECT_NE(nullptr, ret.get());
    auto firstBaseCtx = this->pCompilerInterface->getFclBaseTranslationCtx();
    EXPECT_NE(nullptr, firstBaseCtx);

    MockDevice md;
    auto ret2 = this->pCompilerInterface->createFclTranslationCtx(md, IGC::CodeType::oclC, IGC::CodeType::spirV);
    EXPECT_NE(nullptr, ret2.get());
    EXPECT_EQ(firstBaseCtx, this->pCompilerInterface->getFclBaseTranslationCtx());
}

TEST_F(CompilerInterfaceTest, GivenRequestForNewFclTranslationCtxWhenDeviceCtxIsAlreadyAvailableThenUseItToReturnValidTranslationCtx) {
    auto device = this->pDevice;
    auto deviceCtx = CIF::RAII::UPtr(new MockCompilerDeviceCtx<MockFclOclDeviceCtx, MockFclOclTranslationCtx>);
    this->pCompilerInterface->setFclDeviceCtx(*device, deviceCtx.get());
    auto ret = this->pCompilerInterface->createFclTranslationCtx(*device, IGC::CodeType::oclC, IGC::CodeType::spirV);
    EXPECT_NE(nullptr, ret.get());
    EXPECT_EQ(deviceCtx->returned, ret.get());
}

TEST_F(CompilerInterfaceTest, GivenSimultaneousRequestForNewFclTranslationContextsWhenDeviceCtxIsNotAlreadyAvailableThenSynchronizeToCreateOnlyOneNewDeviceCtx) {
    auto device = this->pDevice;

    using ListenerT = LockListener<IGC::FclOclDeviceCtxTagOCL, MockFclOclDeviceCtx>;
    ListenerT listenerData(device);
    this->pCompilerInterface->lockListenerData = &listenerData;
    this->pCompilerInterface->lockListener = ListenerT::listener;

    EXPECT_EQ(0, listenerData.lockCount);
    auto ret = this->pCompilerInterface->createFclTranslationCtx(*device, IGC::CodeType::oclC, IGC::CodeType::spirV);
    EXPECT_EQ(2, listenerData.lockCount);
    EXPECT_NE(nullptr, ret.get());
    ASSERT_EQ(1U, this->pCompilerInterface->getFclDeviceContexts().size());
    ASSERT_NE(this->pCompilerInterface->getFclDeviceContexts().end(),
              this->pCompilerInterface->getFclDeviceContexts().find(device));
    EXPECT_NE(nullptr, listenerData.createdDeviceCtx);
    EXPECT_EQ(listenerData.createdDeviceCtx, this->pCompilerInterface->getFclDeviceContexts()[device].get());

    WasLockedListener wasLockedListenerData;
    this->pCompilerInterface->lockListenerData = &wasLockedListenerData;
    this->pCompilerInterface->lockListener = WasLockedListener::listener;
    ret = this->pCompilerInterface->createFclTranslationCtx(*device, IGC::CodeType::spirV, IGC::CodeType::oclGenBin);
    EXPECT_NE(nullptr, ret.get());
    ASSERT_EQ(1U, this->pCompilerInterface->getFclDeviceContexts().size());
    EXPECT_TRUE(wasLockedListenerData.wasLocked);
}

TEST_F(CompilerInterfaceTest, GivenRequestForNewTranslationCtxWhenFclMainIsNotAvailableThenReturnNullptr) {
    NEO::failCreateCifMain = true;

    auto device = this->pDevice;
    MockCompilerInterface tempCompilerInterface;
    auto retFcl = tempCompilerInterface.createFclTranslationCtx(*device, IGC::CodeType::oclC, IGC::CodeType::spirV);
    EXPECT_EQ(nullptr, retFcl);
    auto retIgc = tempCompilerInterface.createIgcTranslationCtx(*device, IGC::CodeType::oclC, IGC::CodeType::spirV);
    EXPECT_EQ(nullptr, retIgc);

    NEO::failCreateCifMain = false;
}

TEST_F(CompilerInterfaceTest, GivenRequestForNewTranslationCtxWhenCouldNotCreateDeviceCtxThenReturnNullptr) {
    auto device = this->pDevice;

    auto befFclMock = NEO::MockCIFMain::getGlobalCreatorFunc<NEO::MockFclOclDeviceCtx>();
    auto befIgcMock = NEO::MockCIFMain::getGlobalCreatorFunc<NEO::MockIgcOclDeviceCtx>();
    NEO::MockCIFMain::setGlobalCreatorFunc<NEO::MockFclOclDeviceCtx>(nullptr);
    NEO::MockCIFMain::setGlobalCreatorFunc<NEO::MockIgcOclDeviceCtx>(nullptr);

    auto retFcl = pCompilerInterface->createFclTranslationCtx(*device, IGC::CodeType::oclC, IGC::CodeType::spirV);
    EXPECT_EQ(nullptr, retFcl);

    auto retIgc = pCompilerInterface->createIgcTranslationCtx(*device, IGC::CodeType::spirV, IGC::CodeType::elf);
    EXPECT_EQ(nullptr, retIgc);

    auto retFinalizer = pCompilerInterface->createFinalizerTranslationCtx(*device, IGC::CodeType::undefined, IGC::CodeType::elf);
    EXPECT_EQ(nullptr, retFinalizer);

    NEO::MockCIFMain::setGlobalCreatorFunc<NEO::MockFclOclDeviceCtx>(befFclMock);
    NEO::MockCIFMain::setGlobalCreatorFunc<NEO::MockIgcOclDeviceCtx>(befIgcMock);
}

TEST_F(CompilerInterfaceTest, GivenRequestForNewIgcTranslationCtxWhenDeviceCtxIsAlreadyAvailableThenUseItToReturnValidTranslationCtx) {
    auto device = this->pDevice;
    auto deviceCtx = CIF::RAII::UPtr(new MockCompilerDeviceCtx<MockIgcOclDeviceCtx, MockIgcOclTranslationCtx>);
    this->pCompilerInterface->setIgcDeviceCtx(*device, deviceCtx.get());
    auto ret = this->pCompilerInterface->createIgcTranslationCtx(*device, IGC::CodeType::spirV, IGC::CodeType::oclGenBin);
    EXPECT_NE(nullptr, ret.get());
    EXPECT_EQ(deviceCtx->returned, ret.get());
}

TEST_F(CompilerInterfaceTest, GivenRequestForNewFinalizerTranslationCtxWhenDeviceCtxIsAlreadyAvailableThenUseItToReturnValidTranslationCtx) {
    auto device = this->pDevice;
    auto deviceCtx = CIF::RAII::UPtr(new MockCompilerDeviceCtx<MockIgcOclDeviceCtx, MockIgcOclTranslationCtx>);
    this->pCompilerInterface->setFinalizerDeviceCtx(*device, deviceCtx.get());
    auto ret = this->pCompilerInterface->createFinalizerTranslationCtx(*device, IGC::CodeType::spirV, IGC::CodeType::oclGenBin);
    EXPECT_NE(nullptr, ret.get());
    EXPECT_EQ(deviceCtx->returned, ret.get());
}

TEST_F(CompilerInterfaceTest, GivenRequestForNewFinalizerTranslationCtxWhenDeviceCtxIsNotAlreadyAvailableThenCreateNewDeviceCtx) {
    MockCompilerProductHelper *mockCompilerProductHelper = nullptr;
    auto device = this->pDevice;
    {
        auto tmp = std::make_unique<MockCompilerProductHelper>();
        mockCompilerProductHelper = tmp.get();
        device->getRootDeviceEnvironmentRef().compilerProductHelper = std::move(tmp);
    }

    mockCompilerProductHelper->getFinalizerLibraryNameResult = "finalzer_lib";
    this->pCompilerInterface->igcLibraryNameOverride = "";

    auto ret = this->pCompilerInterface->createFinalizerTranslationCtx(*device, IGC::CodeType::spirV, IGC::CodeType::oclGenBin);
    EXPECT_NE(nullptr, ret.get());
}

TEST_F(CompilerInterfaceTest, GivenRequestForNewFinalizerTranslationCtxWhenDeviceCtxIsAlreadyAvailableThenReuseThatDeviceCtx) {
    MockCompilerProductHelper *mockCompilerProductHelper = nullptr;
    auto device = this->pDevice;
    {
        auto tmp = std::make_unique<MockCompilerProductHelper>();
        mockCompilerProductHelper = tmp.get();
        device->getRootDeviceEnvironmentRef().compilerProductHelper = std::move(tmp);
    }

    mockCompilerProductHelper->getFinalizerLibraryNameResult = "finalzer_lib";
    this->pCompilerInterface->igcLibraryNameOverride = "";

    auto ret = this->pCompilerInterface->createFinalizerTranslationCtx(*device, IGC::CodeType::spirV, IGC::CodeType::oclGenBin);
    EXPECT_NE(nullptr, ret.get());

    ret = this->pCompilerInterface->createFinalizerTranslationCtx(*device, IGC::CodeType::spirV, IGC::CodeType::oclGenBin);
    EXPECT_NE(nullptr, ret.get());

    EXPECT_EQ(1U, this->pCompilerInterface->customCompilerLibraries.size());
    EXPECT_EQ(1U, this->pCompilerInterface->finalizerDeviceContexts.size());
}

TEST_F(CompilerInterfaceTest, GivenSimultaneousRequestForNewIgcTranslationContextsWhenDeviceCtxIsNotAlreadyAvailableThenSynchronizeToCreateOnlyOneNewDeviceCtx) {
    auto device = this->pDevice;

    using ListenerT = LockListener<IGC::IgcOclDeviceCtxTagOCL, MockIgcOclDeviceCtx>;
    ListenerT listenerData{device};
    this->pCompilerInterface->lockListenerData = &listenerData;
    this->pCompilerInterface->lockListener = ListenerT::listener;

    auto ret = this->pCompilerInterface->createIgcTranslationCtx(*device, IGC::CodeType::spirV, IGC::CodeType::oclGenBin);
    EXPECT_NE(nullptr, ret.get());
    ASSERT_EQ(1U, this->pCompilerInterface->getIgcDeviceContexts().size());
    ASSERT_NE(this->pCompilerInterface->getIgcDeviceContexts().end(),
              this->pCompilerInterface->getIgcDeviceContexts().find(device));
    EXPECT_NE(nullptr, listenerData.createdDeviceCtx);
    EXPECT_EQ(listenerData.createdDeviceCtx, this->pCompilerInterface->getIgcDeviceContexts()[device].get());

    WasLockedListener wasLockedListenerData;
    this->pCompilerInterface->lockListenerData = &wasLockedListenerData;
    this->pCompilerInterface->lockListener = WasLockedListener::listener;
    ret = this->pCompilerInterface->createIgcTranslationCtx(*device, IGC::CodeType::spirV, IGC::CodeType::oclGenBin);
    EXPECT_NE(nullptr, ret.get());
    ASSERT_EQ(1U, this->pCompilerInterface->getIgcDeviceContexts().size());
    EXPECT_TRUE(wasLockedListenerData.wasLocked);
}

TEST_F(CompilerInterfaceTest, GivenRequestForNewIgcTranslationCtxWhenCouldNotPopulatePlatformInfoThenReturnNullptr) {
    auto device = this->pDevice;

    auto prevDebugVars = getIgcDebugVars();

    for (uint32_t i = 1; i < (1 << 3); ++i) {
        this->pCompilerInterface->getIgcDeviceContexts().clear();

        auto debugVars = prevDebugVars;
        debugVars.failCreatePlatformInterface = (i & 1) != 0;
        debugVars.failCreateIgcFeWaInterface = (i & (1 << 2)) != 0;
        debugVars.failCreateGtSystemInfoInterface = (i & (1 << 1)) != 0;
        setIgcDebugVars(debugVars);

        auto ret = pCompilerInterface->createIgcTranslationCtx(*device, IGC::CodeType::spirV, IGC::CodeType::oclGenBin);
        EXPECT_EQ(nullptr, ret);
    }

    setIgcDebugVars(prevDebugVars);
}

HWTEST_F(CompilerInterfaceTest, givenNoDbgKeyForceUseDifferentPlatformWhenRequestForNewTranslationCtxThenUseDefaultPlatform) {
    auto device = this->pDevice;
    auto retIgc = pCompilerInterface->createIgcTranslationCtx(*device, IGC::CodeType::spirV, IGC::CodeType::oclGenBin);
    EXPECT_NE(nullptr, retIgc);
    IGC::IgcOclDeviceCtxTagOCL *devCtx = pCompilerInterface->peekIgcDeviceCtx(device);
    auto igcPlatform = devCtx->GetPlatformHandle();
    auto igcSysInfo = devCtx->GetGTSystemInfoHandle();

    auto hwInfo = device->getHardwareInfo();
    device->getCompilerProductHelper().adjustHwInfoForIgc(hwInfo);

    EXPECT_EQ(hwInfo.platform.eProductFamily, igcPlatform->GetProductFamily());
    EXPECT_EQ(hwInfo.platform.eRenderCoreFamily, igcPlatform->GetRenderCoreFamily());
    EXPECT_EQ(hwInfo.gtSystemInfo.SliceCount, igcSysInfo->GetSliceCount());
    EXPECT_EQ(hwInfo.gtSystemInfo.SubSliceCount, igcSysInfo->GetSubSliceCount());
    EXPECT_EQ(hwInfo.gtSystemInfo.EUCount, igcSysInfo->GetEUCount());
    EXPECT_EQ(hwInfo.gtSystemInfo.ThreadCount, igcSysInfo->GetThreadCount());
}

HWTEST_F(CompilerInterfaceTest, givenDbgKeyForceUseDifferentPlatformWhenRequestForNewTranslationCtxThenUseDbgKeyPlatform) {
    DebugManagerStateRestore dbgRestore;
    auto dbgProdFamily = defaultHwInfo->platform.eProductFamily;
    std::string dbgPlatformString(hardwarePrefix[dbgProdFamily]);
    const GT_SYSTEM_INFO dbgSystemInfo = hardwareInfoTable[dbgProdFamily]->gtSystemInfo;
    debugManager.flags.ForceCompilerUsePlatform.set(dbgPlatformString);

    auto device = this->pDevice;
    device->getRootDeviceEnvironment().getMutableHardwareInfo()->platform.eProductFamily = IGFX_UNKNOWN;
    device->getRootDeviceEnvironment().getMutableHardwareInfo()->platform.eRenderCoreFamily = IGFX_UNKNOWN_CORE;

    auto retIgc = pCompilerInterface->createIgcTranslationCtx(*device, IGC::CodeType::spirV, IGC::CodeType::oclGenBin);
    EXPECT_NE(nullptr, retIgc);
    IGC::IgcOclDeviceCtxTagOCL *devCtx = pCompilerInterface->peekIgcDeviceCtx(device);
    auto igcPlatform = devCtx->GetPlatformHandle();
    auto igcSysInfo = devCtx->GetGTSystemInfoHandle();

    auto hwInfo = *hardwareInfoTable[dbgProdFamily];
    device->getCompilerProductHelper().adjustHwInfoForIgc(hwInfo);

    EXPECT_EQ(hwInfo.platform.eProductFamily, igcPlatform->GetProductFamily());
    EXPECT_EQ(hwInfo.platform.eRenderCoreFamily, igcPlatform->GetRenderCoreFamily());
    EXPECT_EQ(dbgSystemInfo.SliceCount, igcSysInfo->GetSliceCount());
    EXPECT_EQ(dbgSystemInfo.SubSliceCount, igcSysInfo->GetSubSliceCount());
    EXPECT_EQ(dbgSystemInfo.DualSubSliceCount, igcSysInfo->GetSubSliceCount());
    EXPECT_EQ(dbgSystemInfo.EUCount, igcSysInfo->GetEUCount());
    EXPECT_EQ(dbgSystemInfo.ThreadCount, igcSysInfo->GetThreadCount());
}

TEST_F(CompilerInterfaceTest, GivenCompilerWhenGettingCompilerAvailabilityThenCompilerHasCorrectCapabilities) {
    ASSERT_TRUE(this->pCompilerInterface->defaultIgc.entryPoint && this->pCompilerInterface->fcl.entryPoint);
    EXPECT_TRUE(this->pCompilerInterface->isFclAvailable(nullptr));
    EXPECT_TRUE(this->pCompilerInterface->isIgcAvailable(nullptr));
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::oclC, IGC::CodeType::oclGenBin));
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::oclC, IGC::CodeType::spirV));
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::oclC, IGC::CodeType::llvmBc));
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::oclC, IGC::CodeType::llvmLl));
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::spirV, IGC::CodeType::oclGenBin));
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::llvmBc, IGC::CodeType::oclGenBin));
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::llvmLl, IGC::CodeType::oclGenBin));
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::elf, IGC::CodeType::llvmBc));
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::elf, IGC::CodeType::oclGenBin));

    auto befIgcImain = std::move(this->pCompilerInterface->defaultIgc.entryPoint);
    EXPECT_TRUE(this->pCompilerInterface->isFclAvailable(nullptr));
    EXPECT_FALSE(this->pCompilerInterface->isIgcAvailable(nullptr));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::oclC, IGC::CodeType::oclGenBin));
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::oclC, IGC::CodeType::spirV));
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::oclC, IGC::CodeType::llvmBc));
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::oclC, IGC::CodeType::llvmLl));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::spirV, IGC::CodeType::oclGenBin));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::llvmBc, IGC::CodeType::oclGenBin));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::llvmLl, IGC::CodeType::oclGenBin));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::elf, IGC::CodeType::llvmBc));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::elf, IGC::CodeType::oclGenBin));
    this->pCompilerInterface->defaultIgc.entryPoint = std::move(befIgcImain);

    auto befFclImain = std::move(this->pCompilerInterface->fcl.entryPoint);
    EXPECT_FALSE(this->pCompilerInterface->isFclAvailable(nullptr));
    EXPECT_TRUE(this->pCompilerInterface->isIgcAvailable(nullptr));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::oclC, IGC::CodeType::oclGenBin));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::oclC, IGC::CodeType::spirV));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::oclC, IGC::CodeType::llvmBc));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::oclC, IGC::CodeType::llvmLl));
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::spirV, IGC::CodeType::oclGenBin));
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::llvmBc, IGC::CodeType::oclGenBin));
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::llvmLl, IGC::CodeType::oclGenBin));
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::elf, IGC::CodeType::llvmBc));
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::elf, IGC::CodeType::oclGenBin));
    this->pCompilerInterface->fcl.entryPoint = std::move(befFclImain);

    befIgcImain = std::move(this->pCompilerInterface->defaultIgc.entryPoint);
    befFclImain = std::move(this->pCompilerInterface->fcl.entryPoint);
    EXPECT_FALSE(this->pCompilerInterface->isFclAvailable(nullptr));
    EXPECT_FALSE(this->pCompilerInterface->isIgcAvailable(nullptr));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::oclC, IGC::CodeType::oclGenBin));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::oclC, IGC::CodeType::spirV));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::oclC, IGC::CodeType::llvmBc));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::oclC, IGC::CodeType::llvmLl));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::spirV, IGC::CodeType::oclGenBin));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::llvmBc, IGC::CodeType::oclGenBin));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::llvmLl, IGC::CodeType::oclGenBin));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::elf, IGC::CodeType::llvmBc));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(nullptr, IGC::CodeType::elf, IGC::CodeType::oclGenBin));
    this->pCompilerInterface->defaultIgc.entryPoint = std::move(befIgcImain);
    this->pCompilerInterface->fcl.entryPoint = std::move(befFclImain);
}

TEST_F(CompilerInterfaceTest, whenCompilerIsNotAvailableThenGetSipKernelBinaryFailsGracefully) {
    pCompilerInterface->defaultIgc.entryPoint.reset();
    pCompilerInterface->failLoadIgc = true;

    std::vector<char> sipBinary;
    std::vector<char> stateAreaHeader;
    auto err = pCompilerInterface->getSipKernelBinary(*this->pDevice, SipKernelType::csr, sipBinary, stateAreaHeader);
    EXPECT_EQ(TranslationOutput::ErrorCode::compilerNotAvailable, err);
    EXPECT_EQ(0U, sipBinary.size());
}

TEST_F(CompilerInterfaceTest, whenIgcReturnsErrorThenGetSipKernelBinaryFailsGracefully) {
    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.forceBuildFailure = true;
    gEnvironment->igcPushDebugVars(igcDebugVars);

    std::vector<char> sipBinary;
    std::vector<char> stateAreaHeader;
    auto err = pCompilerInterface->getSipKernelBinary(*this->pDevice, SipKernelType::csr, sipBinary, stateAreaHeader);
    EXPECT_EQ(TranslationOutput::ErrorCode::unknownError, err);
    EXPECT_EQ(0U, sipBinary.size());

    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceTest, whenGetIgcDeviceCtxReturnsNullptrThenGetSipKernelBinaryFailsGracefully) {
    pCompilerInterface->failGetIgcDeviceCtx = true;

    std::vector<char> sipBinary;
    std::vector<char> stateAreaHeader;
    auto err = pCompilerInterface->getSipKernelBinary(*this->pDevice, SipKernelType::csr, sipBinary, stateAreaHeader);
    EXPECT_EQ(TranslationOutput::ErrorCode::unknownError, err);
}

TEST_F(CompilerInterfaceTest, whenEverythingIsOkThenGetSipKernelReturnsIgcsOutputAsSipBinary) {
    MockCompilerDebugVars igcDebugVars;
    retrieveBinaryKernelFilename(igcDebugVars.fileName, "CopyBufferShared_simd32_", ".spv");
    gEnvironment->igcPushDebugVars(igcDebugVars);
    std::vector<char> sipBinary;
    std::vector<char> stateAreaHeader;
    auto err = pCompilerInterface->getSipKernelBinary(*this->pDevice, SipKernelType::csr, sipBinary, stateAreaHeader);
    EXPECT_EQ(TranslationOutput::ErrorCode::success, err);
    EXPECT_NE(0U, sipBinary.size());

    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceTest, whenRequestingSipKernelBinaryThenProperSystemRoutineIsSelectedFromCompiler) {
    MockCompilerDebugVars igcDebugVars;
    gEnvironment->igcPushDebugVars(igcDebugVars);
    std::vector<char> sipBinary;
    std::vector<char> stateAreaHeader;
    auto err = pCompilerInterface->getSipKernelBinary(*this->pDevice, SipKernelType::csr, sipBinary, stateAreaHeader);
    EXPECT_EQ(TranslationOutput::ErrorCode::success, err);
    EXPECT_NE(0U, sipBinary.size());
    EXPECT_EQ(IGC::SystemRoutineType::contextSaveRestore, getIgcDebugVars().typeOfSystemRoutine);

    err = pCompilerInterface->getSipKernelBinary(*this->pDevice, SipKernelType::dbgCsr, sipBinary, stateAreaHeader);
    EXPECT_EQ(TranslationOutput::ErrorCode::success, err);
    EXPECT_NE(0U, sipBinary.size());
    EXPECT_EQ(IGC::SystemRoutineType::debug, getIgcDebugVars().typeOfSystemRoutine);

    err = pCompilerInterface->getSipKernelBinary(*this->pDevice, SipKernelType::dbgCsrLocal, sipBinary, stateAreaHeader);
    EXPECT_EQ(TranslationOutput::ErrorCode::success, err);
    EXPECT_NE(0U, sipBinary.size());
    EXPECT_EQ(IGC::SystemRoutineType::debugSlm, getIgcDebugVars().typeOfSystemRoutine);

    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceTest, WhenRequestingBindlessDebugSipThenProperSystemRoutineIsSelectedFromCompiler) {
    MockCompilerDebugVars igcDebugVars;
    gEnvironment->igcPushDebugVars(igcDebugVars);
    std::vector<char> sipBinary;
    std::vector<char> stateAreaHeader;
    auto err = pCompilerInterface->getSipKernelBinary(*this->pDevice, SipKernelType::csr, sipBinary, stateAreaHeader);
    EXPECT_EQ(TranslationOutput::ErrorCode::success, err);
    EXPECT_NE(0U, sipBinary.size());
    EXPECT_EQ(IGC::SystemRoutineType::contextSaveRestore, getIgcDebugVars().typeOfSystemRoutine);
    EXPECT_EQ(MockCompilerDebugVars::SipAddressingType::bindful, getIgcDebugVars().receivedSipAddressingType);

    err = pCompilerInterface->getSipKernelBinary(*this->pDevice, SipKernelType::dbgCsrLocal, sipBinary, stateAreaHeader);
    EXPECT_EQ(TranslationOutput::ErrorCode::success, err);
    EXPECT_NE(0U, sipBinary.size());
    EXPECT_EQ(IGC::SystemRoutineType::debugSlm, getIgcDebugVars().typeOfSystemRoutine);
    EXPECT_EQ(MockCompilerDebugVars::SipAddressingType::bindful, getIgcDebugVars().receivedSipAddressingType);

    err = pCompilerInterface->getSipKernelBinary(*this->pDevice, SipKernelType::dbgBindless, sipBinary, stateAreaHeader);
    EXPECT_EQ(TranslationOutput::ErrorCode::success, err);
    EXPECT_NE(0U, sipBinary.size());
    EXPECT_EQ(IGC::SystemRoutineType::debug, getIgcDebugVars().typeOfSystemRoutine);
    EXPECT_EQ(MockCompilerDebugVars::SipAddressingType::bindless, getIgcDebugVars().receivedSipAddressingType);

    err = pCompilerInterface->getSipKernelBinary(*this->pDevice, SipKernelType::dbgHeapless, sipBinary, stateAreaHeader);
    EXPECT_EQ(TranslationOutput::ErrorCode::success, err);
    EXPECT_NE(0U, sipBinary.size());
    EXPECT_EQ(IGC::SystemRoutineType::debug, getIgcDebugVars().typeOfSystemRoutine);
    EXPECT_EQ(MockCompilerDebugVars::SipAddressingType::bindful, getIgcDebugVars().receivedSipAddressingType);

    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceTest, whenRequestingInvalidSipKernelBinaryThenErrorIsReturned) {
    MockCompilerDebugVars igcDebugVars;
    gEnvironment->igcPushDebugVars(igcDebugVars);
    std::vector<char> sipBinary;
    std::vector<char> stateAreaHeader;
    auto err = pCompilerInterface->getSipKernelBinary(*this->pDevice, SipKernelType::count, sipBinary, stateAreaHeader);
    EXPECT_EQ(TranslationOutput::ErrorCode::unknownError, err);
    EXPECT_EQ(0U, sipBinary.size());
    EXPECT_EQ(IGC::SystemRoutineType::undefined, getIgcDebugVars().typeOfSystemRoutine);

    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceTest, whenCompilerIsNotAvailableThenGetSpecializationConstantsFails) {
    pCompilerInterface->defaultIgc.entryPoint.reset();
    pCompilerInterface->failLoadIgc = true;

    NEO::SpecConstantInfo sci;
    auto err = pCompilerInterface->getSpecConstantsInfo(*pDevice, ArrayRef<char>{}, sci);
    EXPECT_EQ(TranslationOutput::ErrorCode::compilerNotAvailable, err);
}

TEST_F(CompilerInterfaceTest, givenCompilerInterfacewhenGettingIgcFeaturesAndWorkaroundsThenValidPointerIsReturned) {
    auto igcFeaturesAndWorkarounds = pCompilerInterface->getIgcFeaturesAndWorkarounds(*pDevice);
    EXPECT_NE(igcFeaturesAndWorkarounds, nullptr);
    EXPECT_EQ(igcFeaturesAndWorkarounds->GetMaxOCLParamSize(), 0u);
}

TEST_F(CompilerInterfaceTest, GivenRequestForNewFclTranslationCtxWhenInterfaceVersionAbove4ThenPopulatePlatformInfo) {
    auto device = this->pDevice;
    auto hwInfo = device->getHardwareInfo();
    device->getCompilerProductHelper().adjustHwInfoForIgc(hwInfo);

    auto prevDebugVars = getFclDebugVars();

    auto debugVars = prevDebugVars;
    debugVars.overrideFclDeviceCtxVersion = 5;

    setFclDebugVars(debugVars);

    auto ret = pCompilerInterface->createFclTranslationCtx(*device, IGC::CodeType::oclC, IGC::CodeType::spirV);
    ASSERT_NE(nullptr, ret);
    ASSERT_EQ(1U, pCompilerInterface->fclDeviceContexts.size());
    auto platform = pCompilerInterface->fclDeviceContexts.begin()->second->GetPlatformHandle();
    ASSERT_NE(nullptr, platform);
    EXPECT_EQ(hwInfo.platform.eProductFamily, platform->GetProductFamily());

    setFclDebugVars(prevDebugVars);
}

TEST_F(CompilerInterfaceTest, GivenRequestForNewFclTranslationCtxWhenCouldNotPopulatePlatformInfoAndInterfaceVersionAbove4ThenReturnNullptr) {
    auto device = this->pDevice;

    auto prevDebugVars = getFclDebugVars();

    auto debugVars = prevDebugVars;
    debugVars.failCreatePlatformInterface = true;
    debugVars.overrideFclDeviceCtxVersion = 5;

    setFclDebugVars(debugVars);

    auto ret = pCompilerInterface->createFclTranslationCtx(*device, IGC::CodeType::oclC, IGC::CodeType::spirV);
    EXPECT_EQ(nullptr, ret);

    setFclDebugVars(prevDebugVars);
}

TEST_F(CompilerInterfaceTest, GivenRequestForNewFclTranslationCtxWhenInterfaceVersion4ThenDontPopulatePlatformInfo) {
    auto device = this->pDevice;

    auto prevDebugVars = getFclDebugVars();

    auto debugVars = prevDebugVars;
    debugVars.failCreatePlatformInterface = true;
    debugVars.overrideFclDeviceCtxVersion = 4;

    setFclDebugVars(debugVars);

    auto ret = pCompilerInterface->createFclTranslationCtx(*device, IGC::CodeType::oclC, IGC::CodeType::spirV);
    EXPECT_NE(nullptr, ret);

    setFclDebugVars(prevDebugVars);
}

struct SpecConstantsTranslationCtxMock {
    bool returnFalse = false;

    CIF::Builtins::BufferSimple *receivedSrc = nullptr;
    CIF::Builtins::BufferSimple *receivedOutSpecConstantsIds = nullptr;
    CIF::Builtins::BufferSimple *receivedOutSpecConstantsSizes = nullptr;

    bool GetSpecConstantsInfoImpl(CIFBuffer *src, CIFBuffer *outSpecConstantsIds, CIFBuffer *outSpecConstantsSizes) { // NOLINT(readability-identifier-naming)
        this->receivedSrc = src;
        this->receivedOutSpecConstantsIds = outSpecConstantsIds;
        this->receivedOutSpecConstantsSizes = outSpecConstantsSizes;
        return !returnFalse;
    }
};

TEST(GetSpecConstantsTest, givenNullptrTranslationContextAndBuffersWhenGetSpecializationConstantsThenErrorIsReturned) {
    EXPECT_FALSE(NEO::getSpecConstantsInfoImpl<SpecConstantsTranslationCtxMock>(nullptr, nullptr, nullptr, nullptr));
}

TEST(GetSpecConstantsTest, whenGetSpecializationConstantsSuccedThenSuccessIsReturnedAndBuffersArePassed) {
    SpecConstantsTranslationCtxMock tCtxMock;

    auto mockSrc = std::make_unique<MockCIFBuffer>();
    auto mockIds = std::make_unique<MockCIFBuffer>();
    auto mockSizes = std::make_unique<MockCIFBuffer>();

    auto ret = NEO::getSpecConstantsInfoImpl(&tCtxMock, mockSrc.get(), mockIds.get(), mockSizes.get());

    EXPECT_TRUE(ret);
    EXPECT_EQ(mockSrc.get(), tCtxMock.receivedSrc);
    EXPECT_EQ(mockIds.get(), tCtxMock.receivedOutSpecConstantsIds);
    EXPECT_EQ(mockSizes.get(), tCtxMock.receivedOutSpecConstantsSizes);
}

TEST(GetSpecConstantsTest, whenGetSpecializationConstantsFailThenErrorIsReturnedAndBuffersArePassed) {
    SpecConstantsTranslationCtxMock tCtxMock;
    tCtxMock.returnFalse = true;

    auto mockSrc = std::make_unique<MockCIFBuffer>();
    auto mockIds = std::make_unique<MockCIFBuffer>();
    auto mockSizes = std::make_unique<MockCIFBuffer>();

    auto ret = NEO::getSpecConstantsInfoImpl(&tCtxMock, mockSrc.get(), mockIds.get(), mockSizes.get());

    EXPECT_FALSE(ret);
    EXPECT_EQ(mockSrc.get(), tCtxMock.receivedSrc);
    EXPECT_EQ(mockIds.get(), tCtxMock.receivedOutSpecConstantsIds);
    EXPECT_EQ(mockSizes.get(), tCtxMock.receivedOutSpecConstantsSizes);
}

TEST_F(CompilerInterfaceTest, whenIgcTranlationContextCreationFailsThenErrorIsReturned) {
    pCompilerInterface->failCreateIgcTranslationCtx = true;
    NEO::SpecConstantInfo specConstInfo;
    auto err = pCompilerInterface->getSpecConstantsInfo(*pDevice, inputArgs.src, specConstInfo);
    EXPECT_EQ(TranslationOutput::ErrorCode::unknownError, err);
}

TEST_F(CompilerInterfaceTest, givenCompilerInterfaceWhenGetSpecializationConstantsThenSuccessIsReturned) {
    TranslationOutput translationOutput;
    NEO::SpecConstantInfo specConstInfo;
    auto err = pCompilerInterface->getSpecConstantsInfo(*pDevice, inputArgs.src, specConstInfo);
    EXPECT_EQ(TranslationOutput::ErrorCode::success, err);
}

struct UnknownInterfaceCIFMain : MockCIFMain {
    CIF::InterfaceId_t FindIncompatibleImpl(CIF::InterfaceId_t entryPointInterface, CIF::CompatibilityDataHandle handle) const override {
        return CIF::UnknownInterface;
    }
};
struct MockCompilerInterfaceWithUnknownInterfaceCIFMain : MockCompilerInterface {
    bool loadFcl() override {
        CompilerInterface::loadFcl();
        fcl.entryPoint.reset(new UnknownInterfaceCIFMain());
        if (failLoadFcl) {
            return false;
        }
        return true;
    }

    bool loadIgc() {
        CompilerInterface::loadIgcBasedCompiler(defaultIgc, Os::igcDllName);
        defaultIgc.entryPoint.reset(new UnknownInterfaceCIFMain());
        if (failLoadIgc) {
            return false;
        }
        return true;
    }
};

TEST(TestCompilerInterface, givenOptionsWhenCallDisableZebinThenProperOptionsAreSet) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableDebugBreak.set(0);
    debugManager.flags.PrintDebugMessages.set(0);

    auto dummyValid = new MockCIFMain();
    auto mockCompilerInterface = std::make_unique<MockCompilerInterface>();

    mockCompilerInterface->defaultIgc.entryPoint.reset(dummyValid);

    std::string options = "";
    std::string internalOptions = "";
    EXPECT_TRUE(mockCompilerInterface->disableZebin(options, internalOptions));
    EXPECT_TRUE(CompilerOptions::contains(internalOptions, NEO::CompilerOptions::disableZebin.str()));
}

TEST(TranslationOutput, givenNonEmptyPointerAndSizeWhenMakingCopyThenCloneInputData) {
    MockCIFBuffer src;
    src.data.assign({2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37});

    std::string dstString;
    TranslationOutput::makeCopy(dstString, &src);
    ASSERT_EQ(src.GetSize<char>(), dstString.size());
    EXPECT_EQ(0, memcmp(src.GetMemory<void>(), dstString.c_str(), dstString.size()));

    TranslationOutput::MemAndSize dstBuffer;
    TranslationOutput::makeCopy(dstBuffer, &src);
    ASSERT_EQ(src.GetSize<char>(), dstBuffer.size);
    ASSERT_NE(nullptr, dstBuffer.mem);
    EXPECT_EQ(0, memcmp(src.GetMemory<void>(), dstBuffer.mem.get(), dstBuffer.size));
}

TEST(TranslationOutput, givenNullPointerWhenMakingCopyThenClearOutOutput) {
    MockCIFBuffer src;
    src.data.assign({2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37});

    std::string dstString;
    TranslationOutput::makeCopy(dstString, &src);

    TranslationOutput::MemAndSize dstBuffer;
    TranslationOutput::makeCopy(dstBuffer, &src);

    EXPECT_NE(0U, dstString.size());
    TranslationOutput::makeCopy(dstString, nullptr);
    EXPECT_EQ(0U, dstString.size());

    EXPECT_NE(0U, dstBuffer.size);
    EXPECT_NE(nullptr, dstBuffer.mem);
    TranslationOutput::makeCopy(dstBuffer, nullptr);
    EXPECT_EQ(0U, dstBuffer.size);
    EXPECT_EQ(nullptr, dstBuffer.mem);
}

TEST(TranslationOutput, givenZeroSizeWhenMakingCopyThenClearOutOutput) {
    MockCIFBuffer src;
    src.data.assign({2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37});

    std::string dstString;
    TranslationOutput::makeCopy(dstString, &src);

    TranslationOutput::MemAndSize dstBuffer;
    TranslationOutput::makeCopy(dstBuffer, &src);

    MockCIFBuffer emptySrc;
    EXPECT_NE(0U, dstString.size());
    TranslationOutput::makeCopy(dstString, &emptySrc);
    EXPECT_EQ(0U, dstString.size());

    EXPECT_NE(0U, dstBuffer.size);
    EXPECT_NE(nullptr, dstBuffer.mem);
    TranslationOutput::makeCopy(dstBuffer, &emptySrc);
    EXPECT_EQ(0U, dstBuffer.size);
    EXPECT_EQ(nullptr, dstBuffer.mem);
}

TEST(TranslationOutputAppend, givenEmptyInputThenDontChangeContents) {
    const std::string originalContents = "some text";
    std::string dstString = originalContents;
    TranslationOutput::append(dstString, nullptr, "", 0);
    EXPECT_STREQ(originalContents.c_str(), dstString.c_str());

    dstString = originalContents;
    TranslationOutput::append(dstString, nullptr, " ", 1);
    EXPECT_STREQ(originalContents.c_str(), dstString.c_str());

    dstString = originalContents;
    MockCIFBuffer empty;
    TranslationOutput::append(dstString, &empty, "", 0);
    EXPECT_STREQ(originalContents.c_str(), dstString.c_str());

    dstString = originalContents;
    TranslationOutput::append(dstString, &empty, " ", 1);
    EXPECT_STREQ(originalContents.c_str(), dstString.c_str());
}

TEST(TranslationOutputAppend, givenNonEmptyInputThenConcatenate) {
    const std::string originalContents = "some text";
    const std::string suffix = "newtext";

    std::string dstString = originalContents;
    MockCIFBuffer suffixBuffer;
    suffixBuffer.PushBackRawBytes(suffix.c_str(), suffix.size());
    TranslationOutput::append(dstString, &suffixBuffer, "", 0);
    EXPECT_STREQ((originalContents + suffix).c_str(), dstString.c_str());

    dstString = originalContents;
    const char *nullSep = nullptr;
    TranslationOutput::append(dstString, &suffixBuffer, nullSep, 0);
    EXPECT_STREQ((originalContents + suffix).c_str(), dstString.c_str());

    dstString = originalContents;
    TranslationOutput::append(dstString, &suffixBuffer, "SEP", 3);
    EXPECT_STREQ((originalContents + "SEP" + suffix).c_str(), dstString.c_str());

    dstString = "";
    TranslationOutput::append(dstString, &suffixBuffer, "", 0);
    EXPECT_STREQ(suffix.c_str(), dstString.c_str());

    dstString = "";
    TranslationOutput::append(dstString, &suffixBuffer, "SEP", 3);
    EXPECT_STREQ(suffix.c_str(), dstString.c_str());
}

TEST(getOclCExtensionVersion, whenQueryingVersionOfIntegerDotProductExtensionThenReturns200) {
    cl_version defaultVer = CL_MAKE_VERSION(7, 2, 5);
    cl_version ver = NEO::getOclCExtensionVersion("cl_khr_integer_dot_product", defaultVer);
    cl_version expectedVer = CL_MAKE_VERSION(2, 0, 0);
    EXPECT_EQ(expectedVer, ver);
}

TEST(getOclCExtensionVersion, whenQueryingVersionOfUnifiedSharedMemoryExtensionThenReturns110) {
    cl_version defaultVer = CL_MAKE_VERSION(7, 2, 5);
    cl_version ver = NEO::getOclCExtensionVersion("cl_intel_unified_shared_memory", defaultVer);
    cl_version expectedVer = CL_MAKE_VERSION(1, 1, 0);
    EXPECT_EQ(expectedVer, ver);
}

TEST(getOclCExtensionVersion, whenCheckingVersionOfExternalMemoryExtensionThenReturns091) {
    cl_version defaultVer = CL_MAKE_VERSION(7, 2, 5);
    cl_version ver = NEO::getOclCExtensionVersion("cl_khr_external_memory", defaultVer);
    cl_version expectedVer = CL_MAKE_VERSION(0, 9, 1);
    EXPECT_EQ(expectedVer, ver);
}

TEST(getOclCExtensionVersion, whenCheckingVersionOfUntrackedExtensionThenReturnsDefaultValue) {
    cl_version defaultVer = CL_MAKE_VERSION(7, 2, 5);
    cl_version ver = NEO::getOclCExtensionVersion("other", defaultVer);
    EXPECT_EQ(defaultVer, ver);
}