/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/compiler_interface/compiler_interface.h"
#include "core/compiler_interface/compiler_interface.inl"
#include "core/helpers/file_io.h"
#include "core/helpers/hw_info.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/global_environment.h"
#include "unit_tests/helpers/test_files.h"
#include "unit_tests/mocks/mock_cif.h"
#include "unit_tests/mocks/mock_compilers.h"

#include "gmock/gmock.h"

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
        DeviceFixture::SetUp();

        // create the compiler interface
        this->pCompilerInterface = new MockCompilerInterface();
        bool initRet = pCompilerInterface->initialize(std::make_unique<CompilerCache>(CompilerCacheConfig{}), true);
        ASSERT_TRUE(initRet);
        pDevice->getExecutionEnvironment()->compilerInterface.reset(pCompilerInterface);

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

        DeviceFixture::TearDown();
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
        bool initialize(std::unique_ptr<CompilerCache> cache, bool requireFcl) override {
            return false;
        }
    };
    EXPECT_EQ(nullptr, CompilerInterface::createInstance<FailInitializeCompilerInterface>(std::make_unique<CompilerCache>(CompilerCacheConfig{}), false));
}

TEST_F(CompilerInterfaceTest, WhenCompilingToIsaThenSuccessIsReturned) {
    TranslationOutput translationOutput;
    auto err = pCompilerInterface->build(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::Success, err);
}

TEST_F(CompilerInterfaceTest, WhenPrefferedIntermediateRepresentationSpecifiedThenPreserveIt) {
    TranslationOutput translationOutput;
    inputArgs.preferredIntermediateType = IGC::CodeType::llvmLl;
    auto err = pCompilerInterface->build(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(IGC::CodeType::llvmLl, translationOutput.intermediateCodeType);
    EXPECT_EQ(TranslationOutput::ErrorCode::Success, err);
}

TEST_F(CompilerInterfaceTest, WhenBuildIsInvokedThenFclReceivesListOfExtensionsInInternalOptions) {
    std::string receivedInternalOptions;

    auto debugVars = NEO::getFclDebugVars();
    debugVars.receivedInternalOptionsOutput = &receivedInternalOptions;
    gEnvironment->fclPushDebugVars(debugVars);
    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->build(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::Success, err);
    EXPECT_THAT(receivedInternalOptions, testing::HasSubstr(pClDevice->peekCompilerExtensions()));
    gEnvironment->fclPopDebugVars();
}

TEST_F(CompilerInterfaceTest, whenCompilerIsNotAvailableThenBuildFailsGracefully) {
    pCompilerInterface->igcMain.reset(nullptr);
    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->build(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::CompilerNotAvailable, err);
}

TEST_F(CompilerInterfaceTest, whenFclTranslatorReturnsNullptrThenBuildFailsGracefully) {
    pCompilerInterface->failCreateFclTranslationCtx = true;
    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->build(*pDevice, inputArgs, translationOutput);
    pCompilerInterface->failCreateFclTranslationCtx = false;
    EXPECT_EQ(TranslationOutput::ErrorCode::UnknownError, err);
}

TEST_F(CompilerInterfaceTest, whenIgcTranslatorReturnsNullptrThenBuildFailsGracefully) {
    pCompilerInterface->failCreateIgcTranslationCtx = true;
    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->build(*pDevice, inputArgs, translationOutput);
    pCompilerInterface->failCreateIgcTranslationCtx = true;
    EXPECT_EQ(TranslationOutput::ErrorCode::UnknownError, err);
}

TEST_F(CompilerInterfaceTest, GivenOptionsWhenCompilingToIsaThenSuccessIsReturned) {
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
    EXPECT_EQ(TranslationOutput::ErrorCode::Success, err);

    gEnvironment->fclPopDebugVars();
    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceTest, WhenCompilingToIrThenSuccessIsReturned) {
    MockCompilerDebugVars fclDebugVars;
    retrieveBinaryKernelFilename(fclDebugVars.fileName, "CopyBuffer_simd16_", ".bc");
    gEnvironment->fclPushDebugVars(fclDebugVars);
    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->compile(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::Success, err);

    gEnvironment->fclPopDebugVars();
}

TEST_F(CompilerInterfaceTest, GivenProgramCreatedFromIrWhenCompileIsCalledThenDontRecompile) {
    TranslationOutput translationOutput = {};
    inputArgs.srcType = IGC::CodeType::spirV;
    auto err = pCompilerInterface->compile(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::AlreadyCompiled, err);

    inputArgs.srcType = IGC::CodeType::llvmBc;
    err = pCompilerInterface->compile(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::AlreadyCompiled, err);

    inputArgs.srcType = IGC::CodeType::llvmLl;
    err = pCompilerInterface->compile(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::AlreadyCompiled, err);

    inputArgs.srcType = IGC::CodeType::oclGenBin;
    err = pCompilerInterface->compile(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::AlreadyCompiled, err);
}

TEST_F(CompilerInterfaceTest, WhenCompileIsInvokedThenFclReceivesListOfExtensionsInInternalOptions) {
    std::string receivedInternalOptions;

    MockCompilerDebugVars fclDebugVars;
    retrieveBinaryKernelFilename(fclDebugVars.fileName, "CopyBuffer_simd16_", ".bc");
    fclDebugVars.receivedInternalOptionsOutput = &receivedInternalOptions;
    gEnvironment->fclPushDebugVars(fclDebugVars);
    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->compile(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::Success, err);
    EXPECT_THAT(receivedInternalOptions, testing::HasSubstr(pClDevice->peekCompilerExtensions()));
    gEnvironment->fclPopDebugVars();
}

TEST_F(CompilerInterfaceTest, whenCompilerIsNotAvailableThenCompileFailsGracefully) {
    MockCompilerDebugVars fclDebugVars;
    fclDebugVars.fileName = clFiles + "copybuffer.elf";
    gEnvironment->fclPushDebugVars(fclDebugVars);
    pCompilerInterface->igcMain->Release();
    pCompilerInterface->SetIgcMain(nullptr);
    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->compile(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::CompilerNotAvailable, err);

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
    EXPECT_EQ(TranslationOutput::ErrorCode::UnknownError, err);

    gEnvironment->fclPopDebugVars();
}

TEST_F(CompilerInterfaceTest, GivenForceBuildFailureWhenCompilingToIrThenCompilationFailureErrorIsReturned) {
    MockCompilerDebugVars fclDebugVars;
    fclDebugVars.fileName = "../copybuffer.elf";
    fclDebugVars.forceBuildFailure = true;
    gEnvironment->fclPushDebugVars(fclDebugVars);
    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->compile(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::CompilationFailure, err);

    gEnvironment->fclPopDebugVars();
}

TEST_F(CompilerInterfaceTest, GivenForceBuildFailureWhenLinkingIrThenLinkFailureErrorIsReturned) {
    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.fileName = "../copybuffer.ll";
    igcDebugVars.forceBuildFailure = true;
    gEnvironment->igcPushDebugVars(igcDebugVars);
    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->link(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::LinkFailure, err);

    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceTest, WhenLinkIsCalledThenLlvmBcIsUsedAsIntermediateRepresentation) {
    // link only from .ll to gen ISA
    MockCompilerDebugVars igcDebugVars;
    retrieveBinaryKernelFilename(igcDebugVars.fileName, "CopyBuffer_simd16_", ".bc");
    gEnvironment->igcPushDebugVars(igcDebugVars);
    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->link(*pDevice, inputArgs, translationOutput);
    gEnvironment->igcPopDebugVars();
    ASSERT_EQ(TranslationOutput::ErrorCode::Success, err);
    ASSERT_EQ(2U, pCompilerInterface->requestedTranslationCtxs.size());

    MockCompilerInterface::TranslationOpT firstTranslation = {IGC::CodeType::elf, IGC::CodeType::llvmBc},
                                          secondTranslation = {IGC::CodeType::llvmBc, IGC::CodeType::oclGenBin};
    EXPECT_EQ(firstTranslation, pCompilerInterface->requestedTranslationCtxs[0]);
    EXPECT_EQ(secondTranslation, pCompilerInterface->requestedTranslationCtxs[1]);
}

TEST_F(CompilerInterfaceTest, whenCompilerIsNotAvailableThenLinkFailsGracefully) {
    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.fileName = clFiles + "copybuffer.ll";
    gEnvironment->igcPushDebugVars(igcDebugVars);
    pCompilerInterface->igcMain->Release();
    pCompilerInterface->SetIgcMain(nullptr);
    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->link(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::CompilerNotAvailable, err);

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
    EXPECT_EQ(TranslationOutput::ErrorCode::UnknownError, err);

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
    EXPECT_EQ(TranslationOutput::ErrorCode::UnknownError, err);

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
    EXPECT_EQ(TranslationOutput::ErrorCode::LinkFailure, err);

    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceTest, WhenCreateLibraryIsCalledThenLlvmBcIsUsedAsIntermediateRepresentation) {
    // create library from .ll to IR
    MockCompilerDebugVars igcDebugVars;
    retrieveBinaryKernelFilename(igcDebugVars.fileName, "CopyBuffer_simd16_", ".bc");
    gEnvironment->igcPushDebugVars(igcDebugVars);
    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->createLibrary(*pDevice, inputArgs, translationOutput);
    gEnvironment->igcPopDebugVars();
    EXPECT_EQ(TranslationOutput::ErrorCode::Success, err);
    ASSERT_EQ(1U, pCompilerInterface->requestedTranslationCtxs.size());

    EXPECT_EQ(IGC::CodeType::llvmBc, pCompilerInterface->requestedTranslationCtxs[0].second);
}

TEST_F(CompilerInterfaceTest, whenCompilerIsNotAvailableThenCreateLibraryFailsGracefully) {
    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.fileName = clFiles + "copybuffer.ll";
    gEnvironment->igcPushDebugVars(igcDebugVars);
    pCompilerInterface->igcMain->Release();
    pCompilerInterface->SetIgcMain(nullptr);
    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->createLibrary(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::CompilerNotAvailable, err);

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
    EXPECT_EQ(TranslationOutput::ErrorCode::UnknownError, err);

    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceTest, GivenForceBuildFailureWhenFclBuildingThenBuildFailureErrorIsReturned) {
    MockCompilerDebugVars fclDebugVars;
    fclDebugVars.forceCreateFailure = false;
    fclDebugVars.forceBuildFailure = true;
    fclDebugVars.forceRegisterFail = false;
    fclDebugVars.fileName = "copybuffer_skl.bc";

    gEnvironment->fclPushDebugVars(fclDebugVars);

    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->build(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::BuildFailure, err);

    gEnvironment->fclPopDebugVars();
}

TEST_F(CompilerInterfaceTest, GivenForceBuildFailureWhenIgcBuildingThenBuildFailureErrorIsReturned) {
    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.forceCreateFailure = false;
    igcDebugVars.forceBuildFailure = true;
    igcDebugVars.forceRegisterFail = false;
    igcDebugVars.fileName = "copybuffer_skl.gen";

    gEnvironment->igcPushDebugVars(igcDebugVars);

    TranslationOutput translationOutput = {};
    auto err = pCompilerInterface->build(*pDevice, inputArgs, translationOutput);
    EXPECT_EQ(TranslationOutput::ErrorCode::BuildFailure, err);

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

    CIF::RAII::UPtr_t<IGC::OclTranslationOutputTagOCL> Translate(CIF::Builtins::BufferSimple *src,
                                                                 CIF::Builtins::BufferSimple *options,
                                                                 CIF::Builtins::BufferSimple *internalOptions,
                                                                 CIF::Builtins::BufferSimple *tracingOptions,
                                                                 uint32_t tracingOptionsCount) { // NOLINT(readability-identifier-naming)
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
    CIF::RAII::UPtr_t<IGC::OclTranslationOutputTagOCL> Translate(CIF::Builtins::BufferSimple *src,
                                                                 CIF::Builtins::BufferSimple *options,
                                                                 CIF::Builtins::BufferSimple *internalOptions,
                                                                 CIF::Builtins::BufferSimple *tracingOptions,
                                                                 uint32_t tracingOptionsCount,
                                                                 void *gtpinInit) { // NOLINT(readability-identifier-naming)
        return this->Translate(src, options, internalOptions, tracingOptions, tracingOptionsCount);
    }
    CIF::RAII::UPtr_t<IGC::OclTranslationOutputTagOCL> Translate(CIF::Builtins::BufferSimple *src,
                                                                 CIF::Builtins::BufferSimple *specConstantsIds,
                                                                 CIF::Builtins::BufferSimple *specConstantsValues,
                                                                 CIF::Builtins::BufferSimple *options,
                                                                 CIF::Builtins::BufferSimple *internalOptions,
                                                                 CIF::Builtins::BufferSimple *tracingOptions,
                                                                 uint32_t tracingOptionsCount,
                                                                 void *gtPinInput) { // NOLINT(readability-identifier-naming)
        return this->Translate(src, options, internalOptions, tracingOptions, tracingOptionsCount);
    }
};

TEST(TranslateTest, whenArgsAreValidAndTranslatorReturnsValidOutputThenValidOutputIsReturned) {
    TranslationCtxMock mockTranslationCtx;
    auto mockSrc = CIF::RAII::UPtr_t<MockCIFBuffer>(new MockCIFBuffer());
    auto mockOpt = CIF::RAII::UPtr_t<MockCIFBuffer>(new MockCIFBuffer());
    auto mockIntOpt = CIF::RAII::UPtr_t<MockCIFBuffer>(new MockCIFBuffer());

    auto ret = NEO::translate(&mockTranslationCtx, mockSrc.get(), mockOpt.get(), mockIntOpt.get());
    EXPECT_NE(nullptr, ret);

    EXPECT_EQ(mockSrc.get(), mockTranslationCtx.receivedSrc);
    EXPECT_EQ(mockOpt.get(), mockTranslationCtx.receivedOpt);
    EXPECT_EQ(mockIntOpt.get(), mockTranslationCtx.receivedIntOpt);
}

TEST(TranslateTest, givenGtPinInputWhenArgsAreValidAndTranslatorReturnsValidOutputThenValidOutputIsReturned) {
    TranslationCtxMock mockTranslationCtx;
    auto mockSrc = CIF::RAII::UPtr_t<MockCIFBuffer>(new MockCIFBuffer());
    auto mockOpt = CIF::RAII::UPtr_t<MockCIFBuffer>(new MockCIFBuffer());
    auto mockIntOpt = CIF::RAII::UPtr_t<MockCIFBuffer>(new MockCIFBuffer());

    auto ret = NEO::translate(&mockTranslationCtx, mockSrc.get(), mockOpt.get(), mockIntOpt.get(), nullptr);
    EXPECT_NE(nullptr, ret);

    EXPECT_EQ(mockSrc.get(), mockTranslationCtx.receivedSrc);
    EXPECT_EQ(mockOpt.get(), mockTranslationCtx.receivedOpt);
    EXPECT_EQ(mockIntOpt.get(), mockTranslationCtx.receivedIntOpt);
}

TEST(TranslateTest, whenArgsAreInvalidThenNullptrIsReturned) {
    auto mockSrc = CIF::RAII::UPtr_t<MockCIFBuffer>(new MockCIFBuffer());
    auto mockOpt = CIF::RAII::UPtr_t<MockCIFBuffer>(new MockCIFBuffer());
    auto mockIntOpt = CIF::RAII::UPtr_t<MockCIFBuffer>(new MockCIFBuffer());

    auto ret = NEO::translate<TranslationCtxMock>(nullptr, mockSrc.get(), mockOpt.get(), mockIntOpt.get());

    EXPECT_EQ(nullptr, ret);
}

TEST(TranslateTest, givenGtPinInputWhenArgsAreInvalidThenNullptrIsReturned) {
    auto mockSrc = CIF::RAII::UPtr_t<MockCIFBuffer>(new MockCIFBuffer());
    auto mockOpt = CIF::RAII::UPtr_t<MockCIFBuffer>(new MockCIFBuffer());
    auto mockIntOpt = CIF::RAII::UPtr_t<MockCIFBuffer>(new MockCIFBuffer());

    auto ret = NEO::translate<TranslationCtxMock>(nullptr, mockSrc.get(), mockOpt.get(), mockIntOpt.get(), nullptr);

    EXPECT_EQ(nullptr, ret);
}

TEST(TranslateTest, whenTranslatorReturnsNullptrThenNullptrIsReturned) {
    TranslationCtxMock mockTranslationCtx;
    mockTranslationCtx.returnNullptr = true;
    auto mockCifBuffer = CIF::RAII::UPtr_t<MockCIFBuffer>(new MockCIFBuffer());

    auto ret = NEO::translate(&mockTranslationCtx, mockCifBuffer.get(), mockCifBuffer.get(), mockCifBuffer.get());
    EXPECT_EQ(nullptr, ret);
}

TEST(TranslateTest, givenSpecConstantsBuffersWhenTranslatorReturnsNullptrThenNullptrIsReturned) {
    TranslationCtxMock mockTranslationCtx;
    mockTranslationCtx.returnNullptr = true;
    auto mockCifBuffer = CIF::RAII::UPtr_t<MockCIFBuffer>(new MockCIFBuffer());

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
    auto mockCifBuffer = CIF::RAII::UPtr_t<MockCIFBuffer>(new MockCIFBuffer());
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
    auto mockCifBuffer = CIF::RAII::UPtr_t<MockCIFBuffer>(new MockCIFBuffer());
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
    auto mockCifBuffer = CIF::RAII::UPtr_t<MockCIFBuffer>(new MockCIFBuffer());
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
    auto mockCifBuffer = CIF::RAII::UPtr_t<MockCIFBuffer>(new MockCIFBuffer());
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
    MockCompilerEnableGuard mock;
    std::unique_ptr<NEO::OsLibrary> retLib;
    CIF::RAII::UPtr_t<CIF::CIFMain> retMain;
    bool retVal = loadCompiler<IGC::IgcOclDeviceCtx>("", retLib, retMain);
    EXPECT_TRUE(retVal);
    EXPECT_NE(nullptr, retLib.get());
    EXPECT_NE(nullptr, retMain.get());
}

TEST(LoadCompilerTest, whenCouldNotLoadLibraryThenReturnFalseAndNullOutputs) {
    MockCompilerEnableGuard mock;
    std::unique_ptr<NEO::OsLibrary> retLib;
    CIF::RAII::UPtr_t<CIF::CIFMain> retMain;
    bool retVal = loadCompiler<IGC::IgcOclDeviceCtx>("falseName.notRealLib", retLib, retMain);
    EXPECT_FALSE(retVal);
    EXPECT_EQ(nullptr, retLib.get());
    EXPECT_EQ(nullptr, retMain.get());
}

TEST(LoadCompilerTest, whenCreateMainFailsThenReturnFalseAndNullOutputs) {
    NEO::failCreateCifMain = true;

    MockCompilerEnableGuard mock;
    std::unique_ptr<NEO::OsLibrary> retLib;
    CIF::RAII::UPtr_t<CIF::CIFMain> retMain;
    bool retVal = loadCompiler<IGC::IgcOclDeviceCtx>("", retLib, retMain);
    EXPECT_FALSE(retVal);
    EXPECT_EQ(nullptr, retLib.get());
    EXPECT_EQ(nullptr, retMain.get());

    NEO::failCreateCifMain = false;
}

TEST(LoadCompilerTest, whenEntrypointInterfaceIsNotCompatibleThenReturnFalseAndNullOutputs) {
    MockCompilerEnableGuard mock;
    std::unique_ptr<NEO::OsLibrary> retLib;
    CIF::RAII::UPtr_t<CIF::CIFMain> retMain;
    bool retVal = loadCompiler<IGC::GTSystemInfo>("", retLib, retMain);
    EXPECT_FALSE(retVal);
    EXPECT_EQ(nullptr, retLib.get());
    EXPECT_EQ(nullptr, retMain.get());
}

template <typename DeviceCtxBase, typename TranslationCtx>
struct MockCompilerDeviceCtx : DeviceCtxBase {
    TranslationCtx *CreateTranslationCtxImpl(CIF::Version_t ver, IGC::CodeType::CodeType_t inType,
                                             IGC::CodeType::CodeType_t outType) override { // NOLINT(readability-identifier-naming)
        returned = new TranslationCtx;
        return returned;
    }

    TranslationCtx *returned = nullptr;
};

template <typename DeviceCtx, typename MockDeviceCtx>
struct LockListener {
    LockListener(NEO::Device *device)
        : device(device) {
    }

    static void listener(MockCompilerInterface &compInt) {
        auto data = (LockListener *)compInt.lockListenerData;
        auto deviceCtx = CIF::RAII::UPtr(new MockDeviceCtx);
        EXPECT_TRUE(compInt.getDeviceContexts<DeviceCtx>().empty());
        compInt.setDeviceCtx(*data->device, deviceCtx.get());
        data->createdDeviceCtx = deviceCtx.get();
    }

    NEO::Device *device = nullptr;
    MockDeviceCtx *createdDeviceCtx = nullptr;
};

struct WasLockedListener {
    static void listener(MockCompilerInterface &compInt) {
        auto data = (WasLockedListener *)compInt.lockListenerData;
        data->wasLocked = true;
    }
    bool wasLocked = false;
};

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

    auto ret = this->pCompilerInterface->createFclTranslationCtx(*device, IGC::CodeType::oclC, IGC::CodeType::spirV);
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

    auto retIgc = pCompilerInterface->createIgcTranslationCtx(*device, IGC::CodeType::oclC, IGC::CodeType::spirV);
    EXPECT_EQ(nullptr, retIgc);

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

TEST_F(CompilerInterfaceTest, givenNoDbgKeyForceUseDifferentPlatformWhenRequestForNewTranslationCtxThenUseDefaultPlatform) {
    auto device = this->pDevice;
    auto retIgc = pCompilerInterface->createIgcTranslationCtx(*device, IGC::CodeType::spirV, IGC::CodeType::oclGenBin);
    EXPECT_NE(nullptr, retIgc);
    IGC::IgcOclDeviceCtxTagOCL *devCtx = pCompilerInterface->peekIgcDeviceCtx(device);
    auto igcPlatform = devCtx->GetPlatformHandle();
    auto igcSysInfo = devCtx->GetGTSystemInfoHandle();
    EXPECT_EQ(device->getHardwareInfo().platform.eProductFamily, igcPlatform->GetProductFamily());
    EXPECT_EQ(device->getHardwareInfo().platform.eRenderCoreFamily, igcPlatform->GetRenderCoreFamily());
    EXPECT_EQ(device->getHardwareInfo().gtSystemInfo.SliceCount, igcSysInfo->GetSliceCount());
    EXPECT_EQ(device->getHardwareInfo().gtSystemInfo.SubSliceCount, igcSysInfo->GetSubSliceCount());
    EXPECT_EQ(device->getHardwareInfo().gtSystemInfo.EUCount, igcSysInfo->GetEUCount());
    EXPECT_EQ(device->getHardwareInfo().gtSystemInfo.ThreadCount, igcSysInfo->GetThreadCount());
}

TEST_F(CompilerInterfaceTest, givenDbgKeyForceUseDifferentPlatformWhenRequestForNewTranslationCtxThenUseDbgKeyPlatform) {
    DebugManagerStateRestore dbgRestore;
    auto dbgProdFamily = DEFAULT_TEST_PLATFORM::hwInfo.platform.eProductFamily;
    std::string dbgPlatformString(hardwarePrefix[dbgProdFamily]);
    const PLATFORM dbgPlatform = hardwareInfoTable[dbgProdFamily]->platform;
    const GT_SYSTEM_INFO dbgSystemInfo = hardwareInfoTable[dbgProdFamily]->gtSystemInfo;
    DebugManager.flags.ForceCompilerUsePlatform.set(dbgPlatformString);

    auto device = this->pDevice;
    auto retIgc = pCompilerInterface->createIgcTranslationCtx(*device, IGC::CodeType::spirV, IGC::CodeType::oclGenBin);
    EXPECT_NE(nullptr, retIgc);
    IGC::IgcOclDeviceCtxTagOCL *devCtx = pCompilerInterface->peekIgcDeviceCtx(device);
    auto igcPlatform = devCtx->GetPlatformHandle();
    auto igcSysInfo = devCtx->GetGTSystemInfoHandle();

    EXPECT_EQ(dbgPlatform.eProductFamily, igcPlatform->GetProductFamily());
    EXPECT_EQ(dbgPlatform.eRenderCoreFamily, igcPlatform->GetRenderCoreFamily());
    EXPECT_EQ(dbgSystemInfo.SliceCount, igcSysInfo->GetSliceCount());
    EXPECT_EQ(dbgSystemInfo.SubSliceCount, igcSysInfo->GetSubSliceCount());
    EXPECT_EQ(dbgSystemInfo.EUCount, igcSysInfo->GetEUCount());
    EXPECT_EQ(dbgSystemInfo.ThreadCount, igcSysInfo->GetThreadCount());
}

TEST_F(CompilerInterfaceTest, GivenCompilerWhenGettingCompilerAvailabilityThenCompilerHasCorrectCapabilities) {
    ASSERT_TRUE(this->pCompilerInterface->igcMain && this->pCompilerInterface->fclMain);
    EXPECT_TRUE(this->pCompilerInterface->isFclAvailable());
    EXPECT_TRUE(this->pCompilerInterface->isIgcAvailable());
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::oclC, IGC::CodeType::oclGenBin));
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::oclC, IGC::CodeType::spirV));
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::oclC, IGC::CodeType::llvmBc));
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::oclC, IGC::CodeType::llvmLl));
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::spirV, IGC::CodeType::oclGenBin));
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::llvmBc, IGC::CodeType::oclGenBin));
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::llvmLl, IGC::CodeType::oclGenBin));
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::elf, IGC::CodeType::llvmBc));
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::elf, IGC::CodeType::oclGenBin));

    auto befIgcImain = std::move(this->pCompilerInterface->igcMain);
    EXPECT_TRUE(this->pCompilerInterface->isFclAvailable());
    EXPECT_FALSE(this->pCompilerInterface->isIgcAvailable());
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::oclC, IGC::CodeType::oclGenBin));
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::oclC, IGC::CodeType::spirV));
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::oclC, IGC::CodeType::llvmBc));
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::oclC, IGC::CodeType::llvmLl));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::spirV, IGC::CodeType::oclGenBin));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::llvmBc, IGC::CodeType::oclGenBin));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::llvmLl, IGC::CodeType::oclGenBin));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::elf, IGC::CodeType::llvmBc));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::elf, IGC::CodeType::oclGenBin));
    this->pCompilerInterface->igcMain = std::move(befIgcImain);

    auto befFclImain = std::move(this->pCompilerInterface->fclMain);
    EXPECT_FALSE(this->pCompilerInterface->isFclAvailable());
    EXPECT_TRUE(this->pCompilerInterface->isIgcAvailable());
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::oclC, IGC::CodeType::oclGenBin));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::oclC, IGC::CodeType::spirV));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::oclC, IGC::CodeType::llvmBc));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::oclC, IGC::CodeType::llvmLl));
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::spirV, IGC::CodeType::oclGenBin));
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::llvmBc, IGC::CodeType::oclGenBin));
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::llvmLl, IGC::CodeType::oclGenBin));
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::elf, IGC::CodeType::llvmBc));
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::elf, IGC::CodeType::oclGenBin));
    this->pCompilerInterface->fclMain = std::move(befFclImain);

    befIgcImain = std::move(this->pCompilerInterface->igcMain);
    befFclImain = std::move(this->pCompilerInterface->fclMain);
    EXPECT_FALSE(this->pCompilerInterface->isFclAvailable());
    EXPECT_FALSE(this->pCompilerInterface->isIgcAvailable());
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::oclC, IGC::CodeType::oclGenBin));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::oclC, IGC::CodeType::spirV));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::oclC, IGC::CodeType::llvmBc));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::oclC, IGC::CodeType::llvmLl));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::spirV, IGC::CodeType::oclGenBin));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::llvmBc, IGC::CodeType::oclGenBin));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::llvmLl, IGC::CodeType::oclGenBin));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::elf, IGC::CodeType::llvmBc));
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable(IGC::CodeType::elf, IGC::CodeType::oclGenBin));
    this->pCompilerInterface->igcMain = std::move(befIgcImain);
    this->pCompilerInterface->fclMain = std::move(befFclImain);
}

TEST_F(CompilerInterfaceTest, whenCompilerIsNotAvailableThenGetSipKernelBinaryFailsGracefully) {
    pCompilerInterface->igcMain.reset();
    std::vector<char> sipBinary;
    auto err = pCompilerInterface->getSipKernelBinary(*this->pDevice, SipKernelType::Csr, sipBinary);
    EXPECT_EQ(TranslationOutput::ErrorCode::CompilerNotAvailable, err);
    EXPECT_EQ(0U, sipBinary.size());
}

TEST_F(CompilerInterfaceTest, whenIgcTranslatorReturnsNullptrThenGetSipKernelBinaryFailsGracefully) {
    pCompilerInterface->failCreateIgcTranslationCtx = true;
    std::vector<char> sipBinary;
    auto err = pCompilerInterface->getSipKernelBinary(*this->pDevice, SipKernelType::Csr, sipBinary);
    EXPECT_EQ(TranslationOutput::ErrorCode::UnknownError, err);
    EXPECT_EQ(0U, sipBinary.size());
}

TEST_F(CompilerInterfaceTest, whenIgcTranslatorReturnsBuildErrorThenGetSipKernelBinaryFailsGracefully) {
    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.forceBuildFailure = true;
    gEnvironment->igcPushDebugVars(igcDebugVars);

    std::vector<char> sipBinary;
    auto err = pCompilerInterface->getSipKernelBinary(*this->pDevice, SipKernelType::Csr, sipBinary);
    EXPECT_EQ(TranslationOutput::ErrorCode::UnknownError, err);
    EXPECT_EQ(0U, sipBinary.size());

    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceTest, whenEverythingIsOkThenGetSipKernelReturnsIgcsOutputAsSipBinary) {
    MockCompilerDebugVars igcDebugVars;
    retrieveBinaryKernelFilename(igcDebugVars.fileName, "CopyBuffer_simd16_", ".bc");
    gEnvironment->igcPushDebugVars(igcDebugVars);
    std::vector<char> sipBinary;
    auto err = pCompilerInterface->getSipKernelBinary(*this->pDevice, SipKernelType::Csr, sipBinary);
    EXPECT_EQ(TranslationOutput::ErrorCode::Success, err);
    EXPECT_NE(0U, sipBinary.size());

    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceTest, whenRequestingSipKernelBinaryThenProperInternalOptionsAndSrcAreUsed) {
    std::string receivedInternalOptions;
    std::string receivedInput;

    MockCompilerDebugVars igcDebugVars;
    retrieveBinaryKernelFilename(igcDebugVars.fileName, "CopyBuffer_simd16_", ".bc");
    igcDebugVars.receivedInternalOptionsOutput = &receivedInternalOptions;
    igcDebugVars.receivedInput = &receivedInput;
    gEnvironment->igcPushDebugVars(igcDebugVars);
    std::vector<char> sipBinary;
    auto err = pCompilerInterface->getSipKernelBinary(*this->pDevice, SipKernelType::Csr, sipBinary);
    EXPECT_EQ(TranslationOutput::ErrorCode::Success, err);
    EXPECT_NE(0U, sipBinary.size());
    EXPECT_EQ(0, strcmp(getSipKernelCompilerInternalOptions(SipKernelType::Csr), receivedInternalOptions.c_str()));
    std::string expectedInut = getSipLlSrc(*this->pDevice);
    EXPECT_EQ(0, strcmp(expectedInut.c_str(), receivedInput.c_str()));

    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceTest, whenCompilerIsNotAvailableThenGetSpecializationConstantsFails) {
    pCompilerInterface->igcMain.reset();
    NEO::SpecConstantInfo sci;
    auto err = pCompilerInterface->getSpecConstantsInfo(*pDevice, ArrayRef<char>{}, sci);
    EXPECT_EQ(TranslationOutput::ErrorCode::CompilerNotAvailable, err);
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
    EXPECT_FALSE(NEO::getSpecConstantsInfoImpl<SpecConstantsTranslationCtxMock>(nullptr, nullptr, nullptr, nullptr, nullptr));
}

TEST(GetSpecConstantsTest, whenGetSpecializationConstantsSuccedThenSuccessIsReturnedAndBuffersArePassed) {
    SpecConstantsTranslationCtxMock tCtxMock;

    auto mockSrc = CIF::RAII::UPtr_t<MockCIFBuffer>(new MockCIFBuffer());
    auto mockIds = CIF::RAII::UPtr_t<MockCIFBuffer>(new MockCIFBuffer());
    auto mockSizes = CIF::RAII::UPtr_t<MockCIFBuffer>(new MockCIFBuffer());
    auto mockValues = CIF::RAII::UPtr_t<MockCIFBuffer>(new MockCIFBuffer());

    auto ret = NEO::getSpecConstantsInfoImpl(&tCtxMock, mockSrc.get(), mockIds.get(), mockSizes.get(), mockValues.get());

    EXPECT_TRUE(ret);
    EXPECT_EQ(mockSrc.get(), tCtxMock.receivedSrc);
    EXPECT_EQ(mockIds.get(), tCtxMock.receivedOutSpecConstantsIds);
    EXPECT_EQ(mockSizes.get(), tCtxMock.receivedOutSpecConstantsSizes);
}

TEST(GetSpecConstantsTest, whenGetSpecializationConstantsFailThenErrorIsReturnedAndBuffersArePassed) {
    SpecConstantsTranslationCtxMock tCtxMock;
    tCtxMock.returnFalse = true;

    auto mockSrc = CIF::RAII::UPtr_t<MockCIFBuffer>(new MockCIFBuffer());
    auto mockIds = CIF::RAII::UPtr_t<MockCIFBuffer>(new MockCIFBuffer());
    auto mockSizes = CIF::RAII::UPtr_t<MockCIFBuffer>(new MockCIFBuffer());
    auto mockValues = CIF::RAII::UPtr_t<MockCIFBuffer>(new MockCIFBuffer());

    auto ret = NEO::getSpecConstantsInfoImpl(&tCtxMock, mockSrc.get(), mockIds.get(), mockSizes.get(), mockValues.get());

    EXPECT_FALSE(ret);
    EXPECT_EQ(mockSrc.get(), tCtxMock.receivedSrc);
    EXPECT_EQ(mockIds.get(), tCtxMock.receivedOutSpecConstantsIds);
    EXPECT_EQ(mockSizes.get(), tCtxMock.receivedOutSpecConstantsSizes);
}

TEST_F(CompilerInterfaceTest, whenIgcTranlationContextCreationFailsThenErrorIsReturned) {
    pCompilerInterface->failCreateIgcTranslationCtx = true;
    NEO::SpecConstantInfo specConstInfo;
    auto err = pCompilerInterface->getSpecConstantsInfo(*pDevice, inputArgs.src, specConstInfo);
    EXPECT_EQ(TranslationOutput::ErrorCode::UnknownError, err);
}

TEST_F(CompilerInterfaceTest, givenCompilerInterfaceWhenGetSpecializationConstantsThenSuccesIsReturned) {
    TranslationOutput translationOutput;
    NEO::SpecConstantInfo specConstInfo;
    auto err = pCompilerInterface->getSpecConstantsInfo(*pDevice, inputArgs.src, specConstInfo);
    EXPECT_EQ(TranslationOutput::ErrorCode::Success, err);
}

TEST(TranslationOutput, giveNonEmptyPointerAndSizeMakeCopyWillCloneInputData) {
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

TEST(TranslationOutput, givenNullPointerMakeCopyWillClearOutOutput) {
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

TEST(TranslationOutput, givenZeroSizeMakeCopyWillClearOutOutput) {
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
