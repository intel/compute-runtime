/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/compiler_interface/compiler_interface.h"
#include "runtime/compiler_interface/compiler_interface.inl"
#include "runtime/context/context.h"
#include "runtime/gen_common/hw_cmds.h"
#include "runtime/helpers/file_io.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/platform/platform.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/global_environment.h"
#include "unit_tests/helpers/test_files.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/memory_management.h"
#include "unit_tests/mocks/mock_cif.h"
#include "unit_tests/mocks/mock_compilers.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_program.h"

#include "gmock/gmock.h"

using namespace OCLRT;

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

        retVal = CL_SUCCESS;

        // create the compiler interface
        this->pCompilerInterface = new MockCompilerInterface();
        bool initRet = pCompilerInterface->initialize();
        ASSERT_TRUE(initRet);
        pDevice->getExecutionEnvironment()->compilerInterface.reset(pCompilerInterface);

        std::string testFile;

        testFile.append(clFiles);
        testFile.append("CopyBuffer_simd8.cl");

        sourceSize = loadDataFromFile(
            testFile.c_str(),
            pSource);

        ASSERT_NE(0u, sourceSize);
        ASSERT_NE(nullptr, pSource);

        cl_device_id clDevice = pDevice;
        pContext = Context::create<MockContext>(nullptr, DeviceVector(&clDevice, 1), nullptr, nullptr, retVal);
        pProgram = new Program(*pDevice->getExecutionEnvironment(), pContext, false);

        inputArgs.pInput = (char *)pSource;
        inputArgs.InputSize = (uint32_t)sourceSize;
        inputArgs.pOptions = nullptr;
        inputArgs.OptionsSize = 0;
        inputArgs.pInternalOptions = nullptr;
        inputArgs.InternalOptionsSize = 0;
        inputArgs.pTracingOptions = nullptr;
        inputArgs.TracingOptionsCount = 0;
    }

    void TearDown() override {
        delete pProgram;
        delete pContext;

        deleteDataReadFromFile(pSource);

        DeviceFixture::TearDown();
    }

    MockCompilerInterface *pCompilerInterface;
    TranslationArgs inputArgs;
    Program *pProgram = nullptr;
    MockContext *pContext = nullptr;
    void *pSource = nullptr;
    size_t sourceSize = 0;
    cl_int retVal = CL_SUCCESS;
};

TEST_F(CompilerInterfaceTest, BuildWithDebugData) {
    class MyCompilerInterface : public CompilerInterface {
      public:
        static MyCompilerInterface *allocate() {

            auto compilerInterface = new MyCompilerInterface();
            if (!compilerInterface->initializePub()) {
                delete compilerInterface;
                compilerInterface = nullptr;
            }

            for (size_t n = 0; n < sizeof(compilerInterface->mockDebugData); n++) {
                compilerInterface->mockDebugData[n] = (char)n;
            }

            auto vars = OCLRT::getIgcDebugVars();
            vars.debugDataToReturn = compilerInterface->mockDebugData;
            vars.debugDataToReturnSize = sizeof(compilerInterface->mockDebugData);
            OCLRT::setIgcDebugVars(vars);

            return compilerInterface;
        }

        ~MyCompilerInterface() override {
            auto vars = OCLRT::getIgcDebugVars();
            vars.debugDataToReturn = nullptr;
            vars.debugDataToReturnSize = 0;
            OCLRT::setIgcDebugVars(vars);
        }

        bool initializePub() {
            return initialize();
        }

        char mockDebugData[32];
    };

    // Build a regular program
    cl_device_id device = pDevice;
    char *kernel = (char *)"__kernel void\nCB(\n__global unsigned int* src, __global unsigned int* dst)\n{\nint id = (int)get_global_id(0);\ndst[id] = src[id];\n}\n";
    pProgram->setSource(kernel);
    retVal = pProgram->build(1, &device, nullptr, nullptr, nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Inject DebugData during this build
    class MyCompilerInterface *cip = MyCompilerInterface::allocate();
    EXPECT_NE(nullptr, cip);
    retVal = cip->build(*pProgram, inputArgs, false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Verify
    size_t debugDataSize = 0;
    retVal = pProgram->getInfo(CL_PROGRAM_DEBUG_INFO_SIZES_INTEL, sizeof(debugDataSize), &debugDataSize, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(debugDataSize, sizeof(cip->mockDebugData));

    char *debugData = new char[debugDataSize];
    for (size_t n = 0; n < sizeof(debugData); n++) {
        debugData[n] = 0;
    }
    char *pDebugData = &debugData[0];
    size_t retData = 0;
    bool isOK = true;
    retVal = pProgram->getInfo(CL_PROGRAM_DEBUG_INFO_INTEL, 1, &pDebugData, &retData);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    retVal = pProgram->getInfo(CL_PROGRAM_DEBUG_INFO_INTEL, debugDataSize, &pDebugData, &retData);
    EXPECT_EQ(CL_SUCCESS, retVal);
    cl_uint numDevices;
    retVal = clGetProgramInfo(pProgram, CL_PROGRAM_NUM_DEVICES, sizeof(numDevices), &numDevices, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(numDevices * sizeof(debugData), retData);
    // Check integrity of returned debug data
    for (size_t n = 0; n < debugDataSize; n++) {
        if (debugData[n] != (char)n) {
            isOK = false;
            break;
        }
    }
    EXPECT_TRUE(isOK);
    for (size_t n = debugDataSize; n < sizeof(debugData); n++) {
        if (debugData[n] != (char)0) {
            isOK = false;
            break;
        }
    }
    EXPECT_TRUE(isOK);

    retData = 0;
    retVal = pProgram->getInfo(CL_PROGRAM_DEBUG_INFO_INTEL, debugDataSize, nullptr, &retData);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(numDevices * sizeof(debugData), retData);
    cip->shutdown();

    delete[] debugData;
    delete cip;
}

TEST_F(CompilerInterfaceTest, CompileClToIsa) {
    // build from .cl to gen ISA
    retVal = pCompilerInterface->build(*pProgram, inputArgs, false);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(CompilerInterfaceTest, WhenBuildIsInvokedThenFclReceivesListOfExtensionsInInternalOptions) {
    std::string receivedInternalOptions;

    auto debugVars = OCLRT::getFclDebugVars();
    debugVars.receivedInternalOptionsOutput = &receivedInternalOptions;
    gEnvironment->fclPushDebugVars(debugVars);
    retVal = pCompilerInterface->build(*pProgram, inputArgs, false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_THAT(receivedInternalOptions, testing::HasSubstr(platform()->peekCompilerExtensions()));
    gEnvironment->fclPopDebugVars();
}

TEST_F(CompilerInterfaceTest, whenCompilerIsNotAvailableThenBuildFailsGracefully) {
    pCompilerInterface->GetIgcMain()->Release();
    pCompilerInterface->SetIgcMain(nullptr);
    retVal = pCompilerInterface->build(*pProgram, inputArgs, false);
    EXPECT_EQ(CL_COMPILER_NOT_AVAILABLE, retVal);
}

TEST_F(CompilerInterfaceTest, whenFclTranslatorReturnsNullptrThenBuildFailsGracefully) {
    pCompilerInterface->failCreateFclTranslationCtx = true;
    retVal = pCompilerInterface->build(*pProgram, inputArgs, false);
    pCompilerInterface->failCreateFclTranslationCtx = false;
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
}

TEST_F(CompilerInterfaceTest, whenIgcTranslatorReturnsNullptrThenBuildFailsGracefully) {
    pCompilerInterface->failCreateIgcTranslationCtx = true;
    retVal = pCompilerInterface->build(*pProgram, inputArgs, false);
    pCompilerInterface->failCreateIgcTranslationCtx = true;
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
}

TEST_F(CompilerInterfaceTest, CompileClToIsaWithOptions) {
    // build from .cl to gen ISA
    std::string internalOptions = "SOME_OPTION";

    MockCompilerDebugVars fclDebugVars;
    fclDebugVars.fileName = gEnvironment->fclGetMockFile();
    fclDebugVars.internalOptionsExpected = true;
    gEnvironment->fclPushDebugVars(fclDebugVars);

    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.fileName = gEnvironment->igcGetMockFile();
    igcDebugVars.internalOptionsExpected = true;
    gEnvironment->igcPushDebugVars(igcDebugVars);

    inputArgs.pInternalOptions = internalOptions.c_str();
    inputArgs.InternalOptionsSize = static_cast<uint32_t>(internalOptions.length());

    retVal = pCompilerInterface->build(*pProgram, inputArgs, false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    gEnvironment->fclPopDebugVars();
    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceTest, CompileClToIr) {
    // compile only from .cl to IR
    MockCompilerDebugVars fclDebugVars;
    fclDebugVars.fileName = clFiles + "copybuffer.elf";
    gEnvironment->fclPushDebugVars(fclDebugVars);
    retVal = pCompilerInterface->compile(*pProgram, inputArgs);
    EXPECT_EQ(CL_SUCCESS, retVal);

    gEnvironment->fclPopDebugVars();
}

TEST_F(CompilerInterfaceTest, GivenProgramCreatedFromIrWhenCompileIsCalledThenIrFormatIsPreserved) {
    MockProgram prog(*pDevice->getExecutionEnvironment(), pContext, false);
    prog.programBinaryType = CL_PROGRAM_BINARY_TYPE_INTERMEDIATE;
    prog.isSpirV = true;
    retVal = pCompilerInterface->compile(prog, inputArgs);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(prog.isSpirV);

    prog.isSpirV = false;
    retVal = pCompilerInterface->compile(prog, inputArgs);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(prog.isSpirV);
}

TEST_F(CompilerInterfaceTest, WhenCompileIsInvokedThenFclReceivesListOfExtensionsInInternalOptions) {
    std::string receivedInternalOptions;

    MockCompilerDebugVars fclDebugVars;
    fclDebugVars.fileName = clFiles + "copybuffer.elf";
    fclDebugVars.receivedInternalOptionsOutput = &receivedInternalOptions;
    gEnvironment->fclPushDebugVars(fclDebugVars);
    retVal = pCompilerInterface->compile(*pProgram, inputArgs);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_THAT(receivedInternalOptions, testing::HasSubstr(platform()->peekCompilerExtensions()));
    gEnvironment->fclPopDebugVars();
}

TEST_F(CompilerInterfaceTest, whenCompilerIsNotAvailableThenCompileFailsGracefully) {
    MockCompilerDebugVars fclDebugVars;
    fclDebugVars.fileName = clFiles + "copybuffer.elf";
    gEnvironment->fclPushDebugVars(fclDebugVars);
    pCompilerInterface->GetIgcMain()->Release();
    pCompilerInterface->SetIgcMain(nullptr);
    retVal = pCompilerInterface->compile(*pProgram, inputArgs);
    EXPECT_EQ(CL_COMPILER_NOT_AVAILABLE, retVal);

    gEnvironment->fclPopDebugVars();
}

TEST_F(CompilerInterfaceTest, whenFclTranslatorReturnsNullptrThenCompileFailsGracefully) {
    MockCompilerDebugVars fclDebugVars;
    fclDebugVars.fileName = clFiles + "copybuffer.elf";
    gEnvironment->fclPushDebugVars(fclDebugVars);
    pCompilerInterface->failCreateFclTranslationCtx = true;
    retVal = pCompilerInterface->compile(*pProgram, inputArgs);
    pCompilerInterface->failCreateFclTranslationCtx = false;
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);

    gEnvironment->fclPopDebugVars();
}

TEST_F(CompilerInterfaceTest, CompileClToIrCompileFailure) {
    // compile only from .cl to IR
    MockCompilerDebugVars fclDebugVars;
    fclDebugVars.fileName = "../copybuffer.elf";
    fclDebugVars.forceBuildFailure = true;
    gEnvironment->fclPushDebugVars(fclDebugVars);

    retVal = pCompilerInterface->compile(*pProgram, inputArgs);
    EXPECT_EQ(CL_COMPILE_PROGRAM_FAILURE, retVal);

    gEnvironment->fclPopDebugVars();
}

TEST_F(CompilerInterfaceTest, LinkIrLinkFailure) {
    // link only .ll to gen ISA
    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.fileName = "../copybuffer.ll";
    igcDebugVars.forceBuildFailure = true;
    gEnvironment->igcPushDebugVars(igcDebugVars);

    retVal = pCompilerInterface->link(*pProgram, inputArgs);
    EXPECT_EQ(CL_BUILD_PROGRAM_FAILURE, retVal);

    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceTest, WhenLinkIsCalledThenLlvmBcIsUsedAsIntermediateRepresentation) {
    // link only from .ll to gen ISA
    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.fileName = clFiles + "copybuffer.ll";
    gEnvironment->igcPushDebugVars(igcDebugVars);
    retVal = pCompilerInterface->link(*pProgram, inputArgs);
    gEnvironment->igcPopDebugVars();
    ASSERT_EQ(CL_SUCCESS, retVal);
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
    pCompilerInterface->GetIgcMain()->Release();
    pCompilerInterface->SetIgcMain(nullptr);
    retVal = pCompilerInterface->link(*pProgram, inputArgs);
    EXPECT_EQ(CL_COMPILER_NOT_AVAILABLE, retVal);

    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceTest, whenSrcAllocationFailsThenLinkFailsGracefully) {
    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.fileName = clFiles + "copybuffer.ll";
    gEnvironment->igcPushDebugVars(igcDebugVars);
    MockCIFBuffer::failAllocations = true;
    retVal = pCompilerInterface->link(*pProgram, inputArgs);
    MockCIFBuffer::failAllocations = false;
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);

    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceTest, whenTranslateReturnsNullptrThenLinkFailsGracefully) {
    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.fileName = clFiles + "copybuffer.ll";
    gEnvironment->igcPushDebugVars(igcDebugVars);
    pCompilerInterface->failCreateIgcTranslationCtx = true;
    retVal = pCompilerInterface->link(*pProgram, inputArgs);
    pCompilerInterface->failCreateIgcTranslationCtx = false;
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);

    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceTest, CreateLibFailure) {
    // create library from .ll to IR
    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.fileName = "../copybuffer.ll";
    igcDebugVars.forceBuildFailure = true;
    gEnvironment->igcPushDebugVars(igcDebugVars);

    retVal = pCompilerInterface->createLibrary(*pProgram, inputArgs);
    EXPECT_EQ(CL_BUILD_PROGRAM_FAILURE, retVal);

    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceTest, WhenCreateLibraryIsCalledThenLlvmBcIsUsedAsIntermediateRepresentation) {
    // create library from .ll to IR
    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.fileName = clFiles + "copybuffer.ll";
    gEnvironment->igcPushDebugVars(igcDebugVars);
    retVal = pCompilerInterface->createLibrary(*pProgram, inputArgs);
    gEnvironment->igcPopDebugVars();
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(1U, pCompilerInterface->requestedTranslationCtxs.size());

    EXPECT_EQ(IGC::CodeType::llvmBc, pCompilerInterface->requestedTranslationCtxs[0].second);
}

TEST_F(CompilerInterfaceTest, whenCompilerIsNotAvailableThenCreateLibraryFailsGracefully) {
    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.fileName = clFiles + "copybuffer.ll";
    gEnvironment->igcPushDebugVars(igcDebugVars);
    pCompilerInterface->GetIgcMain()->Release();
    pCompilerInterface->SetIgcMain(nullptr);
    retVal = pCompilerInterface->createLibrary(*pProgram, inputArgs);
    EXPECT_EQ(CL_COMPILER_NOT_AVAILABLE, retVal);

    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceTest, whenIgcTranslatorReturnsNullptrThenCreateLibraryFailsGracefully) {
    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.fileName = clFiles + "copybuffer.ll";
    gEnvironment->igcPushDebugVars(igcDebugVars);
    pCompilerInterface->failCreateIgcTranslationCtx = true;
    retVal = pCompilerInterface->createLibrary(*pProgram, inputArgs);
    pCompilerInterface->failCreateIgcTranslationCtx = false;
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);

    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceTest, fclBuildFailure) {
    MockCompilerDebugVars fclDebugVars;
    fclDebugVars.forceCreateFailure = false;
    fclDebugVars.forceBuildFailure = true;
    fclDebugVars.forceRegisterFail = false;
    fclDebugVars.fileName = "copybuffer_skl.bc";

    gEnvironment->fclPushDebugVars(fclDebugVars);

    // build from .cl to gen ISA
    retVal = pCompilerInterface->build(*pProgram, inputArgs, false);
    EXPECT_EQ(CL_BUILD_PROGRAM_FAILURE, retVal);

    gEnvironment->fclPopDebugVars();
}

TEST_F(CompilerInterfaceTest, igcBuildFailure) {
    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.forceCreateFailure = false;
    igcDebugVars.forceBuildFailure = true;
    igcDebugVars.forceRegisterFail = false;
    igcDebugVars.fileName = "copybuffer_skl.gen";

    gEnvironment->igcPushDebugVars(igcDebugVars);

    // build from .cl to gen ISA
    retVal = pCompilerInterface->build(*pProgram, inputArgs, false);
    EXPECT_EQ(CL_BUILD_PROGRAM_FAILURE, retVal);

    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceTest, CompileAndLinkSpirToIsa) {
    // compile and link from SPIR binary to gen ISA
    MockProgram program(*pDevice->getExecutionEnvironment(), pContext, false);
    char binary[] = "BC\xc0\xde ";
    auto retVal = program.createProgramFromBinary(binary, sizeof(binary));
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = pCompilerInterface->compile(program, inputArgs);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = pCompilerInterface->link(program, inputArgs);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(CompilerInterfaceTest, BuildSpirToIsa) {
    // build from SPIR binary to gen ISA
    MockProgram program(*pDevice->getExecutionEnvironment(), pContext, false);
    char binary[] = "BC\xc0\xde ";
    auto retVal = program.createProgramFromBinary(binary, sizeof(binary));
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = pCompilerInterface->build(program, inputArgs, false);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(CompilerInterfaceTest, BuildSpirvToIsa) {
    // build from SPIR binary to gen ISA
    MockProgram program(*pDevice->getExecutionEnvironment(), pContext, false);
    uint64_t spirv[16] = {0x03022307};
    auto retVal = program.createProgramFromBinary(spirv, sizeof(spirv));
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = pCompilerInterface->build(program, inputArgs, false);
    EXPECT_EQ(CL_SUCCESS, retVal);
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
    CIF::RAII::UPtr_t<IGC::OclTranslationOutputTagOCL> Translate(CIF::Builtins::BufferSimple *src,
                                                                 CIF::Builtins::BufferSimple *options,
                                                                 CIF::Builtins::BufferSimple *internalOptions,
                                                                 CIF::Builtins::BufferSimple *tracingOptions,
                                                                 uint32_t tracingOptionsCount, void *gtpinInit) {
        return this->Translate(src, options, internalOptions, tracingOptions, tracingOptionsCount);
    }
};

TEST(TranslateTest, whenArgsAreValidAndTranslatorReturnsValidOutputThenValidOutputIsReturned) {
    TranslationCtxMock mockTranslationCtx;
    auto mockSrc = CIF::RAII::UPtr_t<MockCIFBuffer>(new MockCIFBuffer());
    auto mockOpt = CIF::RAII::UPtr_t<MockCIFBuffer>(new MockCIFBuffer());
    auto mockIntOpt = CIF::RAII::UPtr_t<MockCIFBuffer>(new MockCIFBuffer());

    auto ret = OCLRT::translate(&mockTranslationCtx, mockSrc.get(), mockOpt.get(), mockIntOpt.get());
    EXPECT_NE(nullptr, ret);

    EXPECT_EQ(mockSrc.get(), mockTranslationCtx.receivedSrc);
    EXPECT_EQ(mockOpt.get(), mockTranslationCtx.receivedOpt);
    EXPECT_EQ(mockIntOpt.get(), mockTranslationCtx.receivedIntOpt);
}

TEST(TranslateTest, whenTranslatorReturnsNullptrThenNullptrIsReturned) {
    TranslationCtxMock mockTranslationCtx;
    mockTranslationCtx.returnNullptr = true;
    auto mockCifBuffer = CIF::RAII::UPtr_t<MockCIFBuffer>(new MockCIFBuffer());

    auto ret = OCLRT::translate(&mockTranslationCtx, mockCifBuffer.get(), mockCifBuffer.get(), mockCifBuffer.get());
    EXPECT_EQ(nullptr, ret);
}

TEST(TranslateTest, givenNullPtrAsGtPinInputWhenTranslatorReturnsNullptrThenNullptrIsReturned) {
    TranslationCtxMock mockTranslationCtx;
    mockTranslationCtx.returnNullptr = true;
    auto mockCifBuffer = std::make_unique<MockCIFBuffer>();

    auto ret = OCLRT::translate(&mockTranslationCtx, mockCifBuffer.get(), mockCifBuffer.get(), mockCifBuffer.get(), nullptr);
    EXPECT_EQ(nullptr, ret);
}

TEST(TranslateTest, whenTranslatorReturnsInvalidOutputThenNullptrIsReturned) {
    TranslationCtxMock mockTranslationCtx;
    auto mockCifBuffer = CIF::RAII::UPtr_t<MockCIFBuffer>(new MockCIFBuffer());
    for (uint32_t i = 1; i <= (1 << 3) - 1; ++i) {
        mockTranslationCtx.returnNullptrDebugData = (i & 1) != 0;
        mockTranslationCtx.returnNullptrLog = (i & (1 << 1)) != 0;
        mockTranslationCtx.returnNullptrOutput = (i & (1 << 2)) != 0;
        auto ret = OCLRT::translate(&mockTranslationCtx, mockCifBuffer.get(), mockCifBuffer.get(), mockCifBuffer.get());
        EXPECT_EQ(nullptr, ret);
    }
}

TEST(TranslateTest, givenNullPtrAsGtPinInputWhenTranslatorReturnsInvalidOutputThenNullptrIsReturned) {
    TranslationCtxMock mockTranslationCtx;
    auto mockCifBuffer = CIF::RAII::UPtr_t<MockCIFBuffer>(new MockCIFBuffer());
    for (uint32_t i = 1; i <= (1 << 3) - 1; ++i) {
        mockTranslationCtx.returnNullptrDebugData = (i & 1) != 0;
        mockTranslationCtx.returnNullptrLog = (i & (1 << 1)) != 0;
        mockTranslationCtx.returnNullptrOutput = (i & (1 << 2)) != 0;
        auto ret = OCLRT::translate(&mockTranslationCtx, mockCifBuffer.get(), mockCifBuffer.get(), mockCifBuffer.get(), nullptr);
        EXPECT_EQ(nullptr, ret);
    }
}

TEST(TranslateTest, whenAnyArgIsNullThenNullptrIsReturnedAndTranslatorIsNotInvoked) {
    TranslationCtxMock mockTranslationCtx;
    auto mockCifBuffer = CIF::RAII::UPtr_t<MockCIFBuffer>(new MockCIFBuffer());
    for (uint32_t i = 0; i < (1 << 3) - 1; ++i) {
        auto src = (i & 1) ? mockCifBuffer.get() : nullptr;
        auto opts = (i & (1 << 1)) ? mockCifBuffer.get() : nullptr;
        auto intOpts = (i & (1 << 2)) ? mockCifBuffer.get() : nullptr;

        auto ret = OCLRT::translate(&mockTranslationCtx, src, opts, intOpts);
        EXPECT_EQ(nullptr, ret);
    }

    EXPECT_EQ(nullptr, mockTranslationCtx.receivedSrc);
    EXPECT_EQ(nullptr, mockTranslationCtx.receivedOpt);
    EXPECT_EQ(nullptr, mockTranslationCtx.receivedIntOpt);
    EXPECT_EQ(nullptr, mockTranslationCtx.receivedTracingOpt);
}

TEST(LoadCompilerTest, whenEverythingIsOkThenReturnsTrueAndValidOutputs) {
    MockCompilerEnableGuard mock;
    std::unique_ptr<OCLRT::OsLibrary> retLib;
    CIF::RAII::UPtr_t<CIF::CIFMain> retMain;
    bool retVal = loadCompiler<IGC::IgcOclDeviceCtx>("", retLib, retMain);
    EXPECT_TRUE(retVal);
    EXPECT_NE(nullptr, retLib.get());
    EXPECT_NE(nullptr, retMain.get());
}

TEST(LoadCompilerTest, whenCouldNotLoadLibraryThenReturnFalseAndNullOutputs) {
    MockCompilerEnableGuard mock;
    std::unique_ptr<OCLRT::OsLibrary> retLib;
    CIF::RAII::UPtr_t<CIF::CIFMain> retMain;
    bool retVal = loadCompiler<IGC::IgcOclDeviceCtx>("falseName.notRealLib", retLib, retMain);
    EXPECT_FALSE(retVal);
    EXPECT_EQ(nullptr, retLib.get());
    EXPECT_EQ(nullptr, retMain.get());
}

TEST(LoadCompilerTest, whenCreateMainFailsThenReturnFalseAndNullOutputs) {
    OCLRT::failCreateCifMain = true;

    MockCompilerEnableGuard mock;
    std::unique_ptr<OCLRT::OsLibrary> retLib;
    CIF::RAII::UPtr_t<CIF::CIFMain> retMain;
    bool retVal = loadCompiler<IGC::IgcOclDeviceCtx>("", retLib, retMain);
    EXPECT_FALSE(retVal);
    EXPECT_EQ(nullptr, retLib.get());
    EXPECT_EQ(nullptr, retMain.get());

    OCLRT::failCreateCifMain = false;
}

TEST(LoadCompilerTest, whenEntrypointInterfaceIsNotCompatibleThenReturnFalseAndNullOutputs) {
    MockCompilerEnableGuard mock;
    std::unique_ptr<OCLRT::OsLibrary> retLib;
    CIF::RAII::UPtr_t<CIF::CIFMain> retMain;
    bool retVal = loadCompiler<IGC::GTSystemInfo>("", retLib, retMain);
    EXPECT_FALSE(retVal);
    EXPECT_EQ(nullptr, retLib.get());
    EXPECT_EQ(nullptr, retMain.get());
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
    LockListener(OCLRT::Device *device)
        : device(device) {
    }

    static void Listener(MockCompilerInterface &compInt) {
        auto data = (LockListener *)compInt.lockListenerData;
        auto deviceCtx = CIF::RAII::UPtr(new MockDeviceCtx);
        EXPECT_TRUE(compInt.getDeviceContexts<DeviceCtx>().empty());
        compInt.setDeviceCtx(*data->device, deviceCtx.get());
        data->createdDeviceCtx = deviceCtx.get();
    }

    OCLRT::Device *device = nullptr;
    MockDeviceCtx *createdDeviceCtx = nullptr;
};

TEST_F(CompilerInterfaceTest, GivenRequestForNewFclTranslationCtxWhenDeviceCtxIsNotAvailableThenCreateNewDeviceCtxAndUseItToReturnValidTranslationCtx) {
    auto device = this->pContext->getDevice(0);
    auto ret = this->pCompilerInterface->createFclTranslationCtx(*device, IGC::CodeType::oclC, IGC::CodeType::spirV);
    EXPECT_NE(nullptr, ret.get());
    auto firstBaseCtx = this->pCompilerInterface->getFclBaseTranslationCtx();
    EXPECT_NE(nullptr, firstBaseCtx);

    MockDevice md{device->getHardwareInfo()};
    auto ret2 = this->pCompilerInterface->createFclTranslationCtx(md, IGC::CodeType::oclC, IGC::CodeType::spirV);
    EXPECT_NE(nullptr, ret2.get());
    EXPECT_EQ(firstBaseCtx, this->pCompilerInterface->getFclBaseTranslationCtx());
}

TEST_F(CompilerInterfaceTest, GivenRequestForNewFclTranslationCtxWhenDeviceCtxIsAlreadyAvailableThenUseItToReturnValidTranslationCtx) {
    auto device = this->pContext->getDevice(0);
    auto deviceCtx = CIF::RAII::UPtr(new MockCompilerDeviceCtx<MockFclOclDeviceCtx, MockFclOclTranslationCtx>);
    this->pCompilerInterface->setFclDeviceCtx(*device, deviceCtx.get());
    auto ret = this->pCompilerInterface->createFclTranslationCtx(*device, IGC::CodeType::oclC, IGC::CodeType::spirV);
    EXPECT_NE(nullptr, ret.get());
    EXPECT_EQ(deviceCtx->returned, ret.get());
}

TEST_F(CompilerInterfaceTest, GivenSimultaneousRequestForNewFclTranslationContextsWhenDeviceCtxIsNotAlreadyAvailableThenSynchronizeToCreateOnlyOneNewDeviceCtx) {
    auto device = this->pContext->getDevice(0);

    using ListenerT = LockListener<IGC::FclOclDeviceCtxTagOCL, MockFclOclDeviceCtx>;
    ListenerT listenerData(device);
    this->pCompilerInterface->lockListenerData = &listenerData;
    this->pCompilerInterface->lockListener = ListenerT::Listener;

    auto ret = this->pCompilerInterface->createFclTranslationCtx(*device, IGC::CodeType::oclC, IGC::CodeType::spirV);
    EXPECT_NE(nullptr, ret.get());
    ASSERT_EQ(1U, this->pCompilerInterface->getFclDeviceContexts().size());
    ASSERT_NE(this->pCompilerInterface->getFclDeviceContexts().end(),
              this->pCompilerInterface->getFclDeviceContexts().find(device));
    EXPECT_NE(nullptr, listenerData.createdDeviceCtx);
    EXPECT_EQ(listenerData.createdDeviceCtx, this->pCompilerInterface->getFclDeviceContexts()[device].get());
}

TEST_F(CompilerInterfaceTest, GivenRequestForNewTranslationCtxWhenFclMainIsNotAvailableThenReturnNullptr) {
    OCLRT::failCreateCifMain = true;

    auto device = this->pContext->getDevice(0);
    MockCompilerInterface tempCompilerInterface;
    auto retFcl = tempCompilerInterface.createFclTranslationCtx(*device, IGC::CodeType::oclC, IGC::CodeType::spirV);
    EXPECT_EQ(nullptr, retFcl);
    auto retIgc = tempCompilerInterface.createIgcTranslationCtx(*device, IGC::CodeType::oclC, IGC::CodeType::spirV);
    EXPECT_EQ(nullptr, retIgc);

    OCLRT::failCreateCifMain = false;
}

TEST_F(CompilerInterfaceTest, GivenRequestForNewTranslationCtxWhenCouldNotCreateDeviceCtxThenReturnNullptr) {
    auto device = this->pContext->getDevice(0);

    auto befFclMock = OCLRT::MockCIFMain::getGlobalCreatorFunc<OCLRT::MockFclOclDeviceCtx>();
    auto befIgcMock = OCLRT::MockCIFMain::getGlobalCreatorFunc<OCLRT::MockIgcOclDeviceCtx>();
    OCLRT::MockCIFMain::setGlobalCreatorFunc<OCLRT::MockFclOclDeviceCtx>(nullptr);
    OCLRT::MockCIFMain::setGlobalCreatorFunc<OCLRT::MockIgcOclDeviceCtx>(nullptr);

    auto retFcl = pCompilerInterface->createFclTranslationCtx(*device, IGC::CodeType::oclC, IGC::CodeType::spirV);
    EXPECT_EQ(nullptr, retFcl);

    auto retIgc = pCompilerInterface->createIgcTranslationCtx(*device, IGC::CodeType::oclC, IGC::CodeType::spirV);
    EXPECT_EQ(nullptr, retIgc);

    OCLRT::MockCIFMain::setGlobalCreatorFunc<OCLRT::MockFclOclDeviceCtx>(befFclMock);
    OCLRT::MockCIFMain::setGlobalCreatorFunc<OCLRT::MockIgcOclDeviceCtx>(befIgcMock);
}

TEST_F(CompilerInterfaceTest, GivenRequestForNewIgcTranslationCtxWhenDeviceCtxIsAlreadyAvailableThenUseItToReturnValidTranslationCtx) {
    auto device = this->pContext->getDevice(0);
    auto deviceCtx = CIF::RAII::UPtr(new MockCompilerDeviceCtx<MockIgcOclDeviceCtx, MockIgcOclTranslationCtx>);
    this->pCompilerInterface->setIgcDeviceCtx(*device, deviceCtx.get());
    auto ret = this->pCompilerInterface->createIgcTranslationCtx(*device, IGC::CodeType::spirV, IGC::CodeType::oclGenBin);
    EXPECT_NE(nullptr, ret.get());
    EXPECT_EQ(deviceCtx->returned, ret.get());
}

TEST_F(CompilerInterfaceTest, GivenSimultaneousRequestForNewIgcTranslationContextsWhenDeviceCtxIsNotAlreadyAvailableThenSynchronizeToCreateOnlyOneNewDeviceCtx) {
    auto device = this->pContext->getDevice(0);

    using ListenerT = LockListener<IGC::IgcOclDeviceCtxTagOCL, MockIgcOclDeviceCtx>;
    ListenerT listenerData{device};
    this->pCompilerInterface->lockListenerData = &listenerData;
    this->pCompilerInterface->lockListener = ListenerT::Listener;

    auto ret = this->pCompilerInterface->createIgcTranslationCtx(*device, IGC::CodeType::spirV, IGC::CodeType::oclGenBin);
    EXPECT_NE(nullptr, ret.get());
    ASSERT_EQ(1U, this->pCompilerInterface->getIgcDeviceContexts().size());
    ASSERT_NE(this->pCompilerInterface->getIgcDeviceContexts().end(),
              this->pCompilerInterface->getIgcDeviceContexts().find(device));
    EXPECT_NE(nullptr, listenerData.createdDeviceCtx);
    EXPECT_EQ(listenerData.createdDeviceCtx, this->pCompilerInterface->getIgcDeviceContexts()[device].get());
}

TEST_F(CompilerInterfaceTest, GivenRequestForNewIgcTranslationCtxWhenCouldNotPopulatePlatformInfoThenReturnNullptr) {
    auto device = this->pContext->getDevice(0);

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
    auto device = this->pContext->getDevice(0);
    auto retIgc = pCompilerInterface->createIgcTranslationCtx(*device, IGC::CodeType::spirV, IGC::CodeType::oclGenBin);
    EXPECT_NE(nullptr, retIgc);
    IGC::IgcOclDeviceCtxTagOCL *devCtx = pCompilerInterface->peekIgcDeviceCtx(device);
    auto igcPlatform = devCtx->GetPlatformHandle();
    auto igcSysInfo = devCtx->GetGTSystemInfoHandle();
    EXPECT_EQ(device->getHardwareInfo().pPlatform->eProductFamily, igcPlatform->GetProductFamily());
    EXPECT_EQ(device->getHardwareInfo().pPlatform->eRenderCoreFamily, igcPlatform->GetRenderCoreFamily());
    EXPECT_EQ(device->getHardwareInfo().pSysInfo->SliceCount, igcSysInfo->GetSliceCount());
    EXPECT_EQ(device->getHardwareInfo().pSysInfo->SubSliceCount, igcSysInfo->GetSubSliceCount());
    EXPECT_EQ(device->getHardwareInfo().pSysInfo->EUCount, igcSysInfo->GetEUCount());
    EXPECT_EQ(device->getHardwareInfo().pSysInfo->ThreadCount, igcSysInfo->GetThreadCount());
}

TEST_F(CompilerInterfaceTest, givenDbgKeyForceUseDifferentPlatformWhenRequestForNewTranslationCtxThenUseDbgKeyPlatform) {
    DebugManagerStateRestore dbgRestore;
    auto dbgProdFamily = DEFAULT_TEST_PLATFORM::hwInfo.pPlatform->eProductFamily;
    std::string dbgPlatformString(hardwarePrefix[dbgProdFamily]);
    const PLATFORM dbgPlatform = *hardwareInfoTable[dbgProdFamily]->pPlatform;
    const GT_SYSTEM_INFO dbgSystemInfo = *hardwareInfoTable[dbgProdFamily]->pSysInfo;
    DebugManager.flags.ForceCompilerUsePlatform.set(dbgPlatformString);

    auto device = this->pContext->getDevice(0);
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

TEST_F(CompilerInterfaceTest, IsCompilerAvailable) {
    ASSERT_TRUE(this->pCompilerInterface->GetIgcMain() && this->pCompilerInterface->GetFclMain());
    EXPECT_TRUE(this->pCompilerInterface->isCompilerAvailable());

    auto befIgcImain = this->pCompilerInterface->GetIgcMain();
    this->pCompilerInterface->SetIgcMain(nullptr);
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable());
    this->pCompilerInterface->SetIgcMain(befIgcImain);

    auto befFclImain = this->pCompilerInterface->GetFclMain();
    this->pCompilerInterface->SetFclMain(nullptr);
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable());
    this->pCompilerInterface->SetFclMain(befIgcImain);

    this->pCompilerInterface->SetIgcMain(nullptr);
    this->pCompilerInterface->SetFclMain(nullptr);
    EXPECT_FALSE(this->pCompilerInterface->isCompilerAvailable());
    this->pCompilerInterface->SetIgcMain(befIgcImain);
    this->pCompilerInterface->SetFclMain(befFclImain);
}

TEST_F(CompilerInterfaceTest, whenCompilerIsNotAvailableThenGetSipKernelBinaryFailsGracefully) {
    pCompilerInterface->GetIgcMain()->Release();
    pCompilerInterface->SetIgcMain(nullptr);
    std::vector<char> sipBinary;
    retVal = pCompilerInterface->getSipKernelBinary(SipKernelType::Csr, *this->pDevice, sipBinary);
    EXPECT_EQ(CL_COMPILER_NOT_AVAILABLE, retVal);
    EXPECT_EQ(0U, sipBinary.size());
}

TEST_F(CompilerInterfaceTest, whenIgcTranslatorReturnsNullptrThenGetSipKernelBinaryFailsGracefully) {
    pCompilerInterface->failCreateIgcTranslationCtx = true;
    std::vector<char> sipBinary;
    retVal = pCompilerInterface->getSipKernelBinary(SipKernelType::Csr, *this->pDevice, sipBinary);
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
    EXPECT_EQ(0U, sipBinary.size());
}

TEST_F(CompilerInterfaceTest, whenIgcTranslatorReturnsBuildErrorThenGetSipKernelBinaryFailsGracefully) {
    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.forceBuildFailure = true;
    gEnvironment->igcPushDebugVars(igcDebugVars);

    std::vector<char> sipBinary;
    retVal = pCompilerInterface->getSipKernelBinary(SipKernelType::Csr, *this->pDevice, sipBinary);
    EXPECT_EQ(CL_BUILD_PROGRAM_FAILURE, retVal);
    EXPECT_EQ(0U, sipBinary.size());

    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceTest, whenEverythingIsOkThenGetSipKernelReturnsIgcsOutputAsSipBinary) {
    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.fileName = clFiles + "copybuffer.ll";
    gEnvironment->igcPushDebugVars(igcDebugVars);
    std::vector<char> sipBinary;
    retVal = pCompilerInterface->getSipKernelBinary(SipKernelType::Csr, *this->pDevice, sipBinary);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(0U, sipBinary.size());

    gEnvironment->igcPopDebugVars();
}

TEST_F(CompilerInterfaceTest, whenRequestingSipKernelBinaryThenProperInternalOptionsAndSrcAreUsed) {
    std::string receivedInternalOptions;
    std::string receivedInput;

    MockCompilerDebugVars igcDebugVars;
    igcDebugVars.fileName = clFiles + "copybuffer.ll";
    igcDebugVars.receivedInternalOptionsOutput = &receivedInternalOptions;
    igcDebugVars.receivedInput = &receivedInput;
    gEnvironment->igcPushDebugVars(igcDebugVars);
    std::vector<char> sipBinary;
    retVal = pCompilerInterface->getSipKernelBinary(SipKernelType::Csr, *this->pDevice, sipBinary);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(0U, sipBinary.size());
    EXPECT_EQ(0, strcmp(getSipKernelCompilerInternalOptions(SipKernelType::Csr), receivedInternalOptions.c_str()));
    std::string expectedInut = getSipLlSrc(*this->pDevice);
    EXPECT_EQ(0, strcmp(expectedInut.c_str(), receivedInput.c_str()));

    gEnvironment->igcPopDebugVars();
}
