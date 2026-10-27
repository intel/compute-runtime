/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device_binary_format/zebin/zebin_elf.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_modules_zebin.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/source/program/leo_program.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/module/module.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

#include "CL/cl.h"

namespace NEO {
namespace LEO {
namespace ult {

namespace {
void CL_CALLBACK dummyProgramCallback(cl_program, void *) {}

// SPIR-V magic word (\x07\x23\x02\x03) followed by padding.
const uint32_t spirvBlob[] = {0x07230203, 0x00010000, 0x0u, 0x0u};
// LLVM bitcode magic ("BC\xc0\xde") followed by padding.
const uint8_t llvmBcBlob[] = {'B', 'C', 0xc0, 0xde, 0x0, 0x0, 0x0, 0x0};
const uint8_t emptyArArchiveBlob[] = {'!', '<', 'a', 'r', 'c', 'h', '>', '\n'};
} // namespace

// Minimal L0::Module stub that counts destroy() calls and never deletes itself,
// so it is safe to route zeModuleDestroy at it from a stack object.
struct MockTrackedModule : public L0::Module {
    uint32_t destroyCalled = 0u;
    bool linkedFully = true;

    ze_result_t destroy() override {
        ++destroyCalled;
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t initialize(const ze_module_desc_t *, NEO::Device *) override { return ZE_RESULT_SUCCESS; }
    L0::Device *getDevice() const override { return nullptr; }
    ze_result_t createKernel(const ze_kernel_desc_t *, ze_kernel_handle_t *) override { return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; }
    ze_result_t getNativeBinary(size_t *, uint8_t *) override { return ZE_RESULT_SUCCESS; }
    ze_result_t getIrBinary(size_t *, uint8_t *) override { return ZE_RESULT_SUCCESS; }
    ze_result_t getFunctionPointer(const char *, void **) override { return ZE_RESULT_SUCCESS; }
    ze_result_t getGlobalPointer(const char *, size_t *, void **) override { return ZE_RESULT_SUCCESS; }
    ze_result_t getKernelNames(uint32_t *, const char **) override { return ZE_RESULT_SUCCESS; }
    ze_result_t getDebugInfo(size_t *, uint8_t *) override { return ZE_RESULT_SUCCESS; }
    ze_result_t getProperties(ze_module_properties_t *) override { return ZE_RESULT_SUCCESS; }
    ze_result_t performDynamicLink(uint32_t, ze_module_handle_t *, ze_module_build_log_handle_t *) override { return ZE_RESULT_SUCCESS; }
    ze_result_t inspectLinkage(ze_linkage_inspection_ext_desc_t *, uint32_t, ze_module_handle_t *, ze_module_build_log_handle_t *) override { return ZE_RESULT_SUCCESS; }
    const L0::KernelImmutableData *getKernelImmutableData(const char *) const override { return nullptr; }
    const std::vector<std::unique_ptr<L0::KernelImmutableData>> &getKernelImmutableDataVector() const override { return emptyKernelImmData; }
    uint32_t getMaxGroupSize(const NEO::KernelDescriptor &) const override { return 0; }
    bool shouldAllocatePrivateMemoryPerDispatch() const override { return false; }
    uint32_t getProfileFlags() const override { return 0; }
    void checkIfPrivateMemoryPerDispatchIsNeeded() override {}
    void populateZebinExtendedArgsMetadata() override {}
    void generateDefaultExtendedArgsMetadata() override {}
    bool isModulesPackage() const override { return false; }
    bool isLinkedFully() const override { return linkedFully; }

    std::vector<std::unique_ptr<L0::KernelImmutableData>> emptyKernelImmData;
};

struct MockIrModule : public MockTrackedModule {
    MockIrModule(const void *ir, size_t irSize) : ir(reinterpret_cast<const uint8_t *>(ir)), irSize(irSize) {}

    ze_result_t getIrBinary(size_t *pSize, uint8_t *pModuleIrBinary) override {
        *pSize = irSize;
        if (pModuleIrBinary) {
            memcpy_s(pModuleIrBinary, irSize, ir, irSize);
        }
        return ZE_RESULT_SUCCESS;
    }

    const uint8_t *ir;
    size_t irSize;
};

struct MockLlvmBcCompileCompilerInterface : public L0::ult::MockCompilerInterface {
    NEO::TranslationErrorCode compile(const NEO::Device &device,
                                      const NEO::TranslationInput &input,
                                      NEO::TranslationOutput &output) override {
        output.intermediateCodeType = IGC::CodeType::llvmBc;
        output.intermediateRepresentation.mem = makeCopy(llvmBcBlob, sizeof(llvmBcBlob));
        output.intermediateRepresentation.size = sizeof(llvmBcBlob);
        return NEO::TranslationErrorCode::success;
    }
};

struct MockEmptyContext : public Context {
    MockEmptyContext() : Context(nullptr, nullptr, 0, nullptr, false) {}
};

struct WhiteBoxProgram : public Program {
    using Program::buildModulesForContextDevices;
    using Program::linkModulesFully;
    using Program::mapModuleBuildResult;
    using Program::moduleHandles;
    using Program::populateIrBinaryFromModule;
    using Program::Program;
    using Program::programBinaryType;
};

using ClProgramCompileLinkTests = Test<OclFixture>;

cl_context createOclContext(Platform *platform, cl_device_id &clDeviceId) {
    auto &clDevices = platform->getDevices();
    EXPECT_FALSE(clDevices.empty());
    if (clDevices.empty()) {
        return nullptr;
    }
    clDeviceId = clDevices[0].get();
    cl_int errcode = CL_SUCCESS;
    cl_context clContext = clCreateContext(nullptr, 1, &clDeviceId, nullptr, nullptr, &errcode);
    EXPECT_EQ(CL_SUCCESS, errcode);
    EXPECT_NE(nullptr, clContext);
    return clContext;
}

TEST_F(ClProgramCompileLinkTests, givenSpirvIlProgramWhenBuildProgramThenOclVersionAndExtensionsReachCompiler) {
    auto *mockCompiler = new L0::ult::MockCompilerInterface();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(mockCompiler);

    cl_device_id clDeviceId = nullptr;
    cl_context clContext = createOclContext(platform, clDeviceId);

    cl_int errcode = CL_SUCCESS;
    auto program = clCreateProgramWithIL(clContext, spirvBlob, sizeof(spirvBlob), &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);

    // Build fails (mock emits no binary) but the compiler still captured the options.
    clBuildProgram(program, 1, &clDeviceId, nullptr, nullptr, nullptr);
    EXPECT_NE(std::string::npos, mockCompiler->inputInternalOptions.find("-ocl-version=300"));
    EXPECT_NE(std::string::npos, mockCompiler->inputInternalOptions.find("-cl-ext="));

    clReleaseProgram(program);
    clReleaseContext(clContext);
}

TEST_F(ClProgramCompileLinkTests, givenSpirvIlProgramsWhenLinkProgramThenOclVersionAndExtensionsReachCompiler) {
    auto *mockCompiler = new L0::ult::MockCompilerInterface();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(mockCompiler);

    cl_device_id clDeviceId = nullptr;
    cl_context clContext = createOclContext(platform, clDeviceId);

    cl_int errcode = CL_SUCCESS;
    auto program1 = clCreateProgramWithIL(clContext, spirvBlob, sizeof(spirvBlob), &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    auto program2 = clCreateProgramWithIL(clContext, spirvBlob, sizeof(spirvBlob), &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);

    cl_program inputPrograms[] = {program1, program2};
    // Failed link still returns a program object; release it.
    auto linkedProgram = clLinkProgram(clContext, 1, &clDeviceId, nullptr, 2, inputPrograms, nullptr, nullptr, &errcode);
    EXPECT_NE(std::string::npos, mockCompiler->inputInternalOptions.find("-ocl-version=300"));
    EXPECT_NE(std::string::npos, mockCompiler->inputInternalOptions.find("-cl-ext="));

    if (linkedProgram != nullptr) {
        clReleaseProgram(linkedProgram);
    }
    clReleaseProgram(program1);
    clReleaseProgram(program2);
    clReleaseContext(clContext);
}

TEST_F(ClProgramCompileLinkTests, givenSpecConstantsSetAroundCompileWhenLinkProgramThenEachInputIsLinkedWithValuesFromItsLastCompile) {
    auto *mockCompiler = new L0::ult::MockCompilerInterfaceWithSpecConstants<uint32_t, uint64_t>(1u);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(mockCompiler);
    const cl_uint specId = mockCompiler->moduleSpecConstantsIds[0];

    cl_device_id clDeviceId = nullptr;
    cl_context clContext = createOclContext(platform, clDeviceId);

    cl_int errcode = CL_SUCCESS;
    auto program1 = clCreateProgramWithIL(clContext, spirvBlob, sizeof(spirvBlob), &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    auto program2 = clCreateProgramWithIL(clContext, spirvBlob, sizeof(spirvBlob), &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);

    auto compileWithSpecConstant = [&](cl_program program, uint64_t value) {
        EXPECT_EQ(CL_SUCCESS, clSetProgramSpecializationConstant(program, specId, sizeof(value), &value));
        EXPECT_EQ(CL_SUCCESS, clCompileProgram(program, 1, &clDeviceId, nullptr, 0, nullptr, nullptr, nullptr, nullptr));
    };
    compileWithSpecConstant(program1, 7u);
    compileWithSpecConstant(program1, 1u);
    compileWithSpecConstant(program2, 2u);
    const uint64_t setAfterCompile = 999u;
    EXPECT_EQ(CL_SUCCESS, clSetProgramSpecializationConstant(program1, specId, sizeof(setAfterCompile), &setAfterCompile));

    cl_program inputPrograms[] = {program1, program2};
    auto linkedProgram = clLinkProgram(clContext, 1, &clDeviceId, nullptr, 2, inputPrograms, nullptr, nullptr, &errcode);

    auto linkedSpecConstants = mockCompiler->getLinkedSpecConstantsPerSpirV();
    ASSERT_EQ(2u, linkedSpecConstants.size());
    EXPECT_EQ((specConstValuesMap{{specId, 1u}}), linkedSpecConstants[0]);
    EXPECT_EQ((specConstValuesMap{{specId, 2u}}), linkedSpecConstants[1]);

    if (linkedProgram != nullptr) {
        clReleaseProgram(linkedProgram);
    }
    clReleaseProgram(program1);
    clReleaseProgram(program2);
    clReleaseContext(clContext);
}

TEST_F(ClProgramCompileLinkTests, givenSpirvIlProgramWhenCompileProgramThenNoModuleIsBuiltAndStaysCompiledObject) {
    cl_device_id clDeviceId = nullptr;
    cl_context clContext = createOclContext(platform, clDeviceId);

    cl_int errcode = CL_SUCCESS;
    auto program = clCreateProgramWithIL(clContext, spirvBlob, sizeof(spirvBlob), &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    auto leoProgram = castToObject<Program>(program);
    ASSERT_NE(nullptr, leoProgram);
    EXPECT_EQ(nullptr, leoProgram->getModuleHandle());

    cl_int result = clCompileProgram(program, 1, &clDeviceId, nullptr, 0, nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, result);
    // Compiling an IL program is a no-op: no gen binary is built ...
    EXPECT_EQ(nullptr, leoProgram->getModuleHandle());
    EXPECT_NE(nullptr, leoProgram->getIrBinary());

    cl_program_binary_type binaryType = CL_PROGRAM_BINARY_TYPE_NONE;
    EXPECT_EQ(CL_SUCCESS, clGetProgramBuildInfo(program, clDeviceId, CL_PROGRAM_BINARY_TYPE, sizeof(binaryType), &binaryType, nullptr));
    EXPECT_EQ(static_cast<cl_program_binary_type>(CL_PROGRAM_BINARY_TYPE_COMPILED_OBJECT), binaryType);

    cl_build_status buildStatus = CL_BUILD_NONE;
    EXPECT_EQ(CL_SUCCESS, clGetProgramBuildInfo(program, clDeviceId, CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, nullptr));
    EXPECT_EQ(static_cast<cl_build_status>(CL_BUILD_SUCCESS), buildStatus);

    clReleaseProgram(program);
    clReleaseContext(clContext);
}

TEST_F(ClProgramCompileLinkTests, givenLlvmBcIlProgramWhenCompileProgramThenLibraryBinaryTypeIsPreserved) {
    cl_device_id clDeviceId = nullptr;
    cl_context clContext = createOclContext(platform, clDeviceId);

    cl_int errcode = CL_SUCCESS;
    auto program = clCreateProgramWithIL(clContext, llvmBcBlob, sizeof(llvmBcBlob), &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);

    cl_int result = clCompileProgram(program, 1, &clDeviceId, nullptr, 0, nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, result);

    // An llvmbc IL program is a library; compile must not relabel it COMPILED_OBJECT.
    cl_program_binary_type binaryType = CL_PROGRAM_BINARY_TYPE_NONE;
    EXPECT_EQ(CL_SUCCESS, clGetProgramBuildInfo(program, clDeviceId, CL_PROGRAM_BINARY_TYPE, sizeof(binaryType), &binaryType, nullptr));
    EXPECT_EQ(static_cast<cl_program_binary_type>(CL_PROGRAM_BINARY_TYPE_LIBRARY), binaryType);

    clReleaseProgram(program);
    clReleaseContext(clContext);
}

TEST_F(ClProgramCompileLinkTests, givenCompilerReturningLlvmBcWhenCompileSourceProgramWithoutCreateLibraryThenCompiledObjectWithLlvmBcIr) {
    auto *mockCompiler = new MockLlvmBcCompileCompilerInterface();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(mockCompiler);

    cl_device_id clDeviceId = nullptr;
    cl_context clContext = createOclContext(platform, clDeviceId);

    cl_int errcode = CL_SUCCESS;
    const char *source = "constant float foo = 9.6F;\n";
    auto program = clCreateProgramWithSource(clContext, 1, &source, nullptr, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    auto leoProgram = castToObject<Program>(program);
    ASSERT_NE(nullptr, leoProgram);

    cl_int result = clCompileProgram(program, 1, &clDeviceId, nullptr, 0, nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_FALSE(leoProgram->getIsSpirv());
    ASSERT_EQ(sizeof(llvmBcBlob), leoProgram->getIrBinarySize());
    EXPECT_EQ(0, memcmp(llvmBcBlob, leoProgram->getIrBinary(), sizeof(llvmBcBlob)));

    cl_program_binary_type binaryType = CL_PROGRAM_BINARY_TYPE_NONE;
    EXPECT_EQ(CL_SUCCESS, clGetProgramBuildInfo(program, clDeviceId, CL_PROGRAM_BINARY_TYPE, sizeof(binaryType), &binaryType, nullptr));
    EXPECT_EQ(static_cast<cl_program_binary_type>(CL_PROGRAM_BINARY_TYPE_COMPILED_OBJECT), binaryType);

    clReleaseProgram(program);
    clReleaseContext(clContext);
}

TEST_F(ClProgramCompileLinkTests, givenInputProgramWithoutIrWhenLinkProgramThenReturnsInvalidOperation) {
    cl_device_id clDeviceId = nullptr;
    cl_context clContext = createOclContext(platform, clDeviceId);

    // A source program that was never compiled holds no IR, so it is not linkable.
    cl_int errcode = CL_SUCCESS;
    const char *source = "kernel void foo() {}";
    auto srcProgram = clCreateProgramWithSource(clContext, 1, &source, nullptr, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);

    cl_program inputPrograms[] = {srcProgram};
    cl_int linkErr = CL_SUCCESS;
    auto linked = clLinkProgram(clContext, 1, &clDeviceId, nullptr, 1, inputPrograms, nullptr, nullptr, &linkErr);
    EXPECT_EQ(CL_INVALID_OPERATION, linkErr);
    // Intentional ICD behavior (matches classic NEO clLinkProgram): on a post-creation link
    // failure the program object is still returned so the caller can query the build log.
    ASSERT_NE(nullptr, linked);

    cl_build_status buildStatus = CL_BUILD_NONE;
    EXPECT_EQ(CL_SUCCESS, clGetProgramBuildInfo(linked, clDeviceId, CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, nullptr));
    EXPECT_EQ(static_cast<cl_build_status>(CL_BUILD_ERROR), buildStatus);

    clReleaseProgram(linked);
    clReleaseProgram(srcProgram);
    clReleaseContext(clContext);
}

TEST_F(ClProgramCompileLinkTests, givenNullInputProgramWhenLinkProgramThenReturnsInvalidProgram) {
    cl_device_id clDeviceId = nullptr;
    cl_context clContext = createOclContext(platform, clDeviceId);

    cl_program inputPrograms[] = {nullptr};
    cl_int linkErr = CL_SUCCESS;
    auto linked = clLinkProgram(clContext, 1, &clDeviceId, nullptr, 1, inputPrograms, nullptr, nullptr, &linkErr);
    EXPECT_EQ(CL_INVALID_PROGRAM, linkErr);
    // Intentional ICD behavior (matches classic NEO clLinkProgram): the program object is still
    // returned on failure so the caller can query the build log.
    ASSERT_NE(nullptr, linked);

    clReleaseProgram(linked);
    clReleaseContext(clContext);
}

TEST(ProgramIsValidCallbackTests, givenNullFuncAndNullUserDataWhenIsValidCallbackThenReturnsTrue) {
    EXPECT_TRUE(Program::isValidCallback(nullptr, nullptr));
}

TEST(ProgramIsValidCallbackTests, givenNonNullFuncAndNullUserDataWhenIsValidCallbackThenReturnsTrue) {
    EXPECT_TRUE(Program::isValidCallback(dummyProgramCallback, nullptr));
}

TEST(ProgramIsValidCallbackTests, givenNonNullFuncAndNonNullUserDataWhenIsValidCallbackThenReturnsTrue) {
    int userData = 42;
    EXPECT_TRUE(Program::isValidCallback(dummyProgramCallback, &userData));
}

TEST(ProgramIsValidCallbackTests, givenNullFuncAndNonNullUserDataWhenIsValidCallbackThenReturnsFalse) {
    int userData = 42;
    EXPECT_FALSE(Program::isValidCallback(nullptr, &userData));
}

TEST(ProgramNullHandleTests, givenNullProgramWhenRetainProgramThenReturnsCLInvalidProgram) {
    EXPECT_EQ(CL_INVALID_PROGRAM, clRetainProgram(nullptr));
}

TEST(ProgramNullHandleTests, givenNullProgramWhenReleaseProgramThenReturnsCLInvalidProgram) {
    EXPECT_EQ(CL_INVALID_PROGRAM, clReleaseProgram(nullptr));
}

TEST(ProgramNullHandleTests, givenNullProgramWhenGetProgramInfoThenReturnsCLInvalidProgram) {
    EXPECT_EQ(CL_INVALID_PROGRAM, clGetProgramInfo(nullptr, CL_PROGRAM_NUM_DEVICES, 0, nullptr, nullptr));
}

TEST(ProgramNullHandleTests, givenNullProgramWhenGetProgramBuildInfoThenReturnsCLInvalidProgram) {
    EXPECT_EQ(CL_INVALID_PROGRAM, clGetProgramBuildInfo(nullptr, nullptr, CL_PROGRAM_BUILD_STATUS, 0, nullptr, nullptr));
}

TEST(ProgramNullHandleTests, givenNullProgramWhenBuildProgramThenReturnsCLInvalidProgram) {
    EXPECT_EQ(CL_INVALID_PROGRAM, clBuildProgram(nullptr, 0, nullptr, nullptr, nullptr, nullptr));
}

TEST(CreateProgramTests, givenNullContextWhenCreateProgramWithSourceThenReturnsCLInvalidContext) {
    cl_int errcode = CL_SUCCESS;
    const char *source = "kernel void foo() {}";
    auto program = clCreateProgramWithSource(nullptr, 1, &source, nullptr, &errcode);
    EXPECT_EQ(nullptr, program);
    EXPECT_EQ(CL_INVALID_CONTEXT, errcode);
}

TEST(CreateProgramTests, givenNullStringsWhenCreateProgramWithSourceThenReturnsCLInvalidValue) {
    cl_int errcode = CL_SUCCESS;
    auto program = clCreateProgramWithSource(nullptr, 1, nullptr, nullptr, &errcode);
    EXPECT_EQ(nullptr, program);
    EXPECT_EQ(CL_INVALID_VALUE, errcode);
}

TEST(CreateProgramTests, givenNullContextWhenCreateProgramWithBinaryThenReturnsInvalidContextAndReleasableProgram) {
    // On a null/invalid context the entry point still returns a program object (ICD behavior);
    // it must be constructed without a context and survive release without dereferencing null.
    cl_int errcode = CL_SUCCESS;
    cl_int binaryStatus = CL_SUCCESS;
    const size_t length = sizeof(spirvBlob);
    const unsigned char *binary = reinterpret_cast<const unsigned char *>(spirvBlob);
    cl_device_id deviceList[] = {nullptr};
    auto program = clCreateProgramWithBinary(nullptr, 1, deviceList, &length, &binary, &binaryStatus, &errcode);
    EXPECT_EQ(CL_INVALID_CONTEXT, errcode);
    EXPECT_EQ(CL_INVALID_CONTEXT, binaryStatus);
    ASSERT_NE(nullptr, program);
    EXPECT_EQ(CL_SUCCESS, clReleaseProgram(program));
}

TEST(CreateProgramTests, givenAnyInputWhenCreateProgramWithBuiltInKernelsThenErrcodeIsSetToInvalidValue) {
    cl_int errcode = CL_SUCCESS;
    auto program = clCreateProgramWithBuiltInKernels(nullptr, 0, nullptr, "", &errcode);
    EXPECT_EQ(nullptr, program);
    EXPECT_EQ(CL_INVALID_VALUE, errcode);
}

TEST(CreateProgramTests, givenNullContextWhenCreateProgramWithILThenReturnsInvalidContextAndReleasableProgram) {
    cl_int errcode = CL_SUCCESS;
    auto program = clCreateProgramWithIL(nullptr, spirvBlob, sizeof(spirvBlob), &errcode);
    EXPECT_EQ(CL_INVALID_CONTEXT, errcode);
    ASSERT_NE(nullptr, program);
    EXPECT_EQ(CL_SUCCESS, clReleaseProgram(program));
}

TEST(ProgramRebuildTests, givenProgramWithExistingModuleWhenBuildModulesForContextDevicesThenPriorModuleIsReleasedAndMapCleared) {
    // A rebuild must start from scratch: any module from a previous build is torn down
    // and the map cleared, rather than the stale module being silently reused.
    MockEmptyContext context;
    WhiteBoxProgram program(&context);
    MockTrackedModule staleModule;
    program.setModuleHandle(0u, staleModule.toHandle());
    ASSERT_EQ(staleModule.toHandle(), program.getModuleHandle());

    ze_module_desc_t moduleDescription = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    // Context exposes no devices, so the rebuild loop creates nothing - only the reset runs.
    auto result = program.buildModulesForContextDevices(moduleDescription, CL_BUILD_PROGRAM_FAILURE);

    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(1u, staleModule.destroyCalled);
    EXPECT_TRUE(program.moduleHandles.empty());
    EXPECT_EQ(nullptr, program.getModuleHandle());
}

TEST(ProgramRebuildTests, givenSourceCompileWhenIrCaptureFailsThenBinaryTypeStaysNone) {
    // Zero-device context: the build loop creates no module, so IR capture finds no module to
    // read and fails. The binary type must remain NONE, not the compiled-object type that would
    // be wrongly set before capture.
    MockEmptyContext context;
    WhiteBoxProgram program(&context);

    auto result = program.compileFromSourceWithHeaders(nullptr, 0, nullptr, nullptr);

    EXPECT_EQ(CL_INVALID_OPERATION, result);
    EXPECT_EQ(static_cast<cl_program_binary_type>(CL_PROGRAM_BINARY_TYPE_NONE), program.programBinaryType);
}

TEST(ProgramLinkModulesTests, givenFullyLinkedModulesWhenLinkModulesFullyThenProgramIsExecutable) {
    MockTrackedModule firstModule;
    MockTrackedModule secondModule;
    MockEmptyContext context;
    WhiteBoxProgram program(&context);
    program.setModuleHandle(0u, firstModule.toHandle());
    program.setModuleHandle(1u, secondModule.toHandle());

    EXPECT_EQ(CL_SUCCESS, program.linkModulesFully());
    EXPECT_EQ(static_cast<cl_program_binary_type>(CL_PROGRAM_BINARY_TYPE_EXECUTABLE), program.programBinaryType);
}

TEST(ProgramLinkModulesTests, givenModuleWithUnresolvedImportsWhenLinkModulesFullyThenReturnsLinkFailureAndBinaryTypeIsNone) {
    MockTrackedModule module;
    module.linkedFully = false;
    MockEmptyContext context;
    WhiteBoxProgram program(&context);
    program.setModuleHandle(0u, module.toHandle());

    EXPECT_EQ(CL_LINK_PROGRAM_FAILURE, program.linkModulesFully());
    EXPECT_EQ(static_cast<cl_program_binary_type>(CL_PROGRAM_BINARY_TYPE_NONE), program.programBinaryType);
}

TEST(ProgramIrCaptureTests, givenModuleWithSpirvIrWhenPopulateIrBinaryFromModuleThenIsSpirvIsTrue) {
    MockIrModule module(spirvBlob, sizeof(spirvBlob));
    MockEmptyContext context;
    WhiteBoxProgram program(&context);
    program.setModuleHandle(0u, module.toHandle());

    EXPECT_EQ(CL_SUCCESS, program.populateIrBinaryFromModule());
    EXPECT_TRUE(program.getIsSpirv());
    ASSERT_EQ(sizeof(spirvBlob), program.getIrBinarySize());
    EXPECT_EQ(0, memcmp(spirvBlob, program.getIrBinary(), sizeof(spirvBlob)));
}

TEST(ProgramIrCaptureTests, givenModuleWithLlvmBcIrWhenPopulateIrBinaryFromModuleThenIsSpirvIsFalse) {
    MockIrModule module(llvmBcBlob, sizeof(llvmBcBlob));
    MockEmptyContext context;
    WhiteBoxProgram program(&context);
    program.setModuleHandle(0u, module.toHandle());

    EXPECT_EQ(CL_SUCCESS, program.populateIrBinaryFromModule());
    EXPECT_FALSE(program.getIsSpirv());
    ASSERT_EQ(sizeof(llvmBcBlob), program.getIrBinarySize());
    EXPECT_EQ(0, memcmp(llvmBcBlob, program.getIrBinary(), sizeof(llvmBcBlob)));
}

TEST(ProgramBuildResultTests, givenModuleBuildFailureWhenMapModuleBuildResultThenReturnsApiSpecificCode) {
    // clBuildProgram / clCompileProgram both surface a build failure, but with their own CL code.
    EXPECT_EQ(CL_BUILD_PROGRAM_FAILURE, WhiteBoxProgram::mapModuleBuildResult(ZE_RESULT_ERROR_MODULE_BUILD_FAILURE, CL_BUILD_PROGRAM_FAILURE));
    EXPECT_EQ(CL_COMPILE_PROGRAM_FAILURE, WhiteBoxProgram::mapModuleBuildResult(ZE_RESULT_ERROR_MODULE_BUILD_FAILURE, CL_COMPILE_PROGRAM_FAILURE));
}

TEST(ProgramBuildResultTests, givenModuleLinkFailureWhenMapModuleBuildResultThenReturnsLinkFailureCode) {
    EXPECT_EQ(CL_LINK_PROGRAM_FAILURE, WhiteBoxProgram::mapModuleBuildResult(ZE_RESULT_ERROR_MODULE_LINK_FAILURE, CL_LINK_PROGRAM_FAILURE));
}

TEST(ProgramBuildResultTests, givenLinkContextBuildFailureWhenMapModuleBuildResultThenStillReportsLinkFailure) {
    // During clLinkProgram a build-side failure (e.g. spec-constant processing) is still a link failure.
    EXPECT_EQ(CL_LINK_PROGRAM_FAILURE, WhiteBoxProgram::mapModuleBuildResult(ZE_RESULT_ERROR_MODULE_BUILD_FAILURE, CL_LINK_PROGRAM_FAILURE));
}

TEST(ProgramBuildResultTests, givenNonBuildErrorWhenMapModuleBuildResultThenUsesGenericMapping) {
    // Errors unrelated to the module build keep their generic ze->cl mapping, ignoring the build code.
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, WhiteBoxProgram::mapModuleBuildResult(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, CL_BUILD_PROGRAM_FAILURE));
    EXPECT_EQ(CL_SUCCESS, WhiteBoxProgram::mapModuleBuildResult(ZE_RESULT_SUCCESS, CL_BUILD_PROGRAM_FAILURE));
}

TEST(ProgramRebuildTests, givenBuildModulesForContextDevicesWhenItSucceedsThenItDoesNotOwnBinaryType) {
    // The build loop only manages module/build-log handles; setting the binary type is the
    // caller's responsibility, so a successful loop must leave the type untouched (NONE here).
    MockEmptyContext context;
    WhiteBoxProgram program(&context);
    ASSERT_EQ(static_cast<cl_program_binary_type>(CL_PROGRAM_BINARY_TYPE_NONE), program.programBinaryType);

    ze_module_desc_t moduleDescription = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    // Zero-device context: the loop creates nothing and returns success without touching the type.
    auto result = program.buildModulesForContextDevices(moduleDescription, CL_BUILD_PROGRAM_FAILURE);

    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(static_cast<cl_program_binary_type>(CL_PROGRAM_BINARY_TYPE_NONE), program.programBinaryType);
}

TEST_F(ClProgramCompileLinkTests, givenBuildSucceededButNoModuleForDeviceWhenQueryGlobalVariableSizeThenReturnsInvalidProgram) {
    cl_device_id clDeviceId = nullptr;
    cl_context clContext = createOclContext(platform, clDeviceId);

    cl_int errcode = CL_SUCCESS;
    const char *source = "kernel void foo() {}";
    auto program = clCreateProgramWithSource(clContext, 1, &source, nullptr, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    auto leoProgram = castToObject<Program>(program);
    ASSERT_NE(nullptr, leoProgram);
    // Mark the program built although no module was ever created for the device.
    leoProgram->setBuildStatus(CL_BUILD_SUCCESS);
    ASSERT_EQ(nullptr, leoProgram->getModuleHandle());

    size_t globalVariablesSize = 0u;
    auto result = clGetProgramBuildInfo(program, clDeviceId, CL_PROGRAM_BUILD_GLOBAL_VARIABLE_TOTAL_SIZE,
                                        sizeof(globalVariablesSize), &globalVariablesSize, nullptr);
    EXPECT_EQ(CL_INVALID_PROGRAM, result);

    clReleaseProgram(program);
    clReleaseContext(clContext);
}

TEST_F(ClProgramCompileLinkTests, givenNativeBinaryWhenCreateProgramWithBinaryThenBuildStatusIsSuccess) {
    cl_device_id clDeviceId = nullptr;
    cl_context clContext = createOclContext(platform, clDeviceId);

    ZebinTestData::ZebinWithL0TestCommonModule zebin(device->getHwInfo());
    const size_t length = zebin.storage.size();
    const unsigned char *binary = zebin.storage.data();

    cl_int errcode = CL_INVALID_VALUE;
    cl_int binaryStatus = CL_INVALID_VALUE;
    auto program = clCreateProgramWithBinary(clContext, 1, &clDeviceId, &length, &binary, &binaryStatus, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    EXPECT_EQ(CL_SUCCESS, binaryStatus);
    ASSERT_NE(nullptr, program);

    cl_build_status buildStatus = CL_BUILD_NONE;
    EXPECT_EQ(CL_SUCCESS, clGetProgramBuildInfo(program, clDeviceId, CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, nullptr));
    EXPECT_EQ(static_cast<cl_build_status>(CL_BUILD_SUCCESS), buildStatus);

    cl_program_binary_type binaryType = CL_PROGRAM_BINARY_TYPE_NONE;
    EXPECT_EQ(CL_SUCCESS, clGetProgramBuildInfo(program, clDeviceId, CL_PROGRAM_BINARY_TYPE, sizeof(binaryType), &binaryType, nullptr));
    EXPECT_EQ(static_cast<cl_program_binary_type>(CL_PROGRAM_BINARY_TYPE_EXECUTABLE), binaryType);

    clReleaseProgram(program);
    clReleaseContext(clContext);
}

TEST_F(ClProgramCompileLinkTests, givenProgramCreatedFromNativeBinaryWhenBuildProgramThenBuildStatusStaysSuccess) {
    cl_device_id clDeviceId = nullptr;
    cl_context clContext = createOclContext(platform, clDeviceId);

    ZebinTestData::ZebinWithL0TestCommonModule zebin(device->getHwInfo());
    const size_t length = zebin.storage.size();
    const unsigned char *binary = zebin.storage.data();

    cl_int errcode = CL_INVALID_VALUE;
    auto program = clCreateProgramWithBinary(clContext, 1, &clDeviceId, &length, &binary, nullptr, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, program);

    EXPECT_EQ(CL_SUCCESS, clBuildProgram(program, 1, &clDeviceId, nullptr, nullptr, nullptr));

    cl_build_status buildStatus = CL_BUILD_NONE;
    EXPECT_EQ(CL_SUCCESS, clGetProgramBuildInfo(program, clDeviceId, CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, nullptr));
    EXPECT_EQ(static_cast<cl_build_status>(CL_BUILD_SUCCESS), buildStatus);

    clReleaseProgram(program);
    clReleaseContext(clContext);
}

TEST_F(ClProgramCompileLinkTests, givenSpirvBinaryWhenCreateProgramWithBinaryThenBuildStatusStaysNone) {
    cl_device_id clDeviceId = nullptr;
    cl_context clContext = createOclContext(platform, clDeviceId);

    const size_t length = sizeof(spirvBlob);
    const unsigned char *binary = reinterpret_cast<const unsigned char *>(spirvBlob);

    cl_int errcode = CL_INVALID_VALUE;
    cl_int binaryStatus = CL_INVALID_VALUE;
    auto program = clCreateProgramWithBinary(clContext, 1, &clDeviceId, &length, &binary, &binaryStatus, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    EXPECT_EQ(CL_SUCCESS, binaryStatus);
    ASSERT_NE(nullptr, program);

    cl_build_status buildStatus = CL_BUILD_SUCCESS;
    EXPECT_EQ(CL_SUCCESS, clGetProgramBuildInfo(program, clDeviceId, CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, nullptr));
    EXPECT_EQ(static_cast<cl_build_status>(CL_BUILD_NONE), buildStatus);

    cl_program_binary_type binaryType = CL_PROGRAM_BINARY_TYPE_NONE;
    EXPECT_EQ(CL_SUCCESS, clGetProgramBuildInfo(program, clDeviceId, CL_PROGRAM_BINARY_TYPE, sizeof(binaryType), &binaryType, nullptr));
    EXPECT_EQ(static_cast<cl_program_binary_type>(CL_PROGRAM_BINARY_TYPE_COMPILED_OBJECT), binaryType);

    clReleaseProgram(program);
    clReleaseContext(clContext);
}

TEST_F(ClProgramCompileLinkTests, givenLlvmBcBinaryWhenCreateProgramWithBinaryThenBuildStatusStaysNone) {
    cl_device_id clDeviceId = nullptr;
    cl_context clContext = createOclContext(platform, clDeviceId);

    const size_t length = sizeof(llvmBcBlob);
    const unsigned char *binary = llvmBcBlob;

    cl_int errcode = CL_INVALID_VALUE;
    auto program = clCreateProgramWithBinary(clContext, 1, &clDeviceId, &length, &binary, nullptr, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, program);

    cl_build_status buildStatus = CL_BUILD_SUCCESS;
    EXPECT_EQ(CL_SUCCESS, clGetProgramBuildInfo(program, clDeviceId, CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, nullptr));
    EXPECT_EQ(static_cast<cl_build_status>(CL_BUILD_NONE), buildStatus);

    cl_program_binary_type binaryType = CL_PROGRAM_BINARY_TYPE_NONE;
    EXPECT_EQ(CL_SUCCESS, clGetProgramBuildInfo(program, clDeviceId, CL_PROGRAM_BINARY_TYPE, sizeof(binaryType), &binaryType, nullptr));
    EXPECT_EQ(static_cast<cl_program_binary_type>(CL_PROGRAM_BINARY_TYPE_LIBRARY), binaryType);

    clReleaseProgram(program);
    clReleaseContext(clContext);
}

TEST_F(ClProgramCompileLinkTests, givenSpirvIlWhenCreateProgramWithIlThenBuildStatusStaysNone) {
    cl_device_id clDeviceId = nullptr;
    cl_context clContext = createOclContext(platform, clDeviceId);

    cl_int errcode = CL_INVALID_VALUE;
    auto program = clCreateProgramWithIL(clContext, spirvBlob, sizeof(spirvBlob), &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, program);

    cl_build_status buildStatus = CL_BUILD_SUCCESS;
    EXPECT_EQ(CL_SUCCESS, clGetProgramBuildInfo(program, clDeviceId, CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, nullptr));
    EXPECT_EQ(static_cast<cl_build_status>(CL_BUILD_NONE), buildStatus);

    clReleaseProgram(program);
    clReleaseContext(clContext);
}

TEST_F(ClProgramCompileLinkTests, givenUndecodableDeviceBinaryWhenCreateProgramWithBinaryThenBuildStatusStaysNone) {
    cl_device_id clDeviceId = nullptr;
    cl_context clContext = createOclContext(platform, clDeviceId);

    const size_t length = sizeof(emptyArArchiveBlob);
    const unsigned char *binary = emptyArArchiveBlob;

    cl_int errcode = CL_SUCCESS;
    cl_int binaryStatus = CL_SUCCESS;
    auto program = clCreateProgramWithBinary(clContext, 1, &clDeviceId, &length, &binary, &binaryStatus, &errcode);
    EXPECT_NE(CL_SUCCESS, errcode);
    EXPECT_NE(CL_SUCCESS, binaryStatus);
    ASSERT_NE(nullptr, program);

    cl_build_status buildStatus = CL_BUILD_SUCCESS;
    EXPECT_EQ(CL_SUCCESS, clGetProgramBuildInfo(program, clDeviceId, CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, nullptr));
    EXPECT_EQ(static_cast<cl_build_status>(CL_BUILD_NONE), buildStatus);

    cl_program_binary_type binaryType = CL_PROGRAM_BINARY_TYPE_EXECUTABLE;
    EXPECT_EQ(CL_SUCCESS, clGetProgramBuildInfo(program, clDeviceId, CL_PROGRAM_BINARY_TYPE, sizeof(binaryType), &binaryType, nullptr));
    EXPECT_EQ(static_cast<cl_program_binary_type>(CL_PROGRAM_BINARY_TYPE_NONE), binaryType);

    clReleaseProgram(program);
    clReleaseContext(clContext);
}

struct MockZebinCompilerInterface : public L0::ult::MockCompilerInterface {
    explicit MockZebinCompilerInterface(const NEO::HardwareInfo &hwInfo) : zebin(hwInfo) {}

    NEO::TranslationErrorCode build(const NEO::Device &device,
                                    const NEO::TranslationInput &input,
                                    NEO::TranslationOutput &output) override {
        return emitZebin(output);
    }
    NEO::TranslationErrorCode link(const NEO::Device &device,
                                   const NEO::TranslationInput &input,
                                   NEO::TranslationOutput &output) override {
        return emitZebin(output);
    }

    NEO::TranslationErrorCode emitZebin(NEO::TranslationOutput &output) {
        output.deviceBinary.mem = makeCopy(zebin.storage.data(), zebin.storage.size());
        output.deviceBinary.size = zebin.storage.size();
        return NEO::TranslationErrorCode::success;
    }

    ZebinTestData::ZebinWithL0TestCommonModule zebin;
};

struct ClProgramNonUniformWorkGroupTests : public ClProgramCompileLinkTests {
    void SetUp() override {
        debugManager.flags.FailBuildProgramWithStatefulAccess.set(0);
        ClProgramCompileLinkTests::SetUp();
        neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(new MockZebinCompilerInterface(device->getHwInfo()));
        clContext = createOclContext(platform, clDeviceId);
    }

    void TearDown() override {
        clReleaseContext(clContext);
        ClProgramCompileLinkTests::TearDown();
    }

    cl_program createIlProgram() {
        cl_int errcode = CL_INVALID_VALUE;
        auto program = clCreateProgramWithIL(clContext, spirvBlob, sizeof(spirvBlob), &errcode);
        EXPECT_EQ(CL_SUCCESS, errcode);
        return program;
    }

    static bool allowsNonUniform(cl_program program) {
        return castToObject<Program>(program)->getAllowNonUniform();
    }

    struct OptionsCase {
        const char *options;
        bool expectedAllowNonUniform;
    };
    static constexpr OptionsCase optionsCases[] = {
        {nullptr, false},
        {"-cl-std=CL1.2", false},
        {"-cl-std=CL2.0", true},
        {"-cl-std=CL3.0", true},
        {"-cl-std=CL3.0 -cl-uniform-work-group-size", false}};

    DebugManagerStateRestore restorer;
    cl_device_id clDeviceId = nullptr;
    cl_context clContext = nullptr;
};

TEST_F(ClProgramNonUniformWorkGroupTests, givenIlProgramWhenBuildProgramThenAllowNonUniformFollowsClStdAndUniformWorkGroupSizeOptions) {
    for (const auto &optionsCase : optionsCases) {
        auto program = createIlProgram();
        ASSERT_NE(nullptr, program);
        EXPECT_FALSE(allowsNonUniform(program));

        EXPECT_EQ(CL_SUCCESS, clBuildProgram(program, 1, &clDeviceId, optionsCase.options, nullptr, nullptr));
        EXPECT_EQ(optionsCase.expectedAllowNonUniform, allowsNonUniform(program)) << (optionsCase.options ? optionsCase.options : "nullptr");

        clReleaseProgram(program);
    }
}

TEST_F(ClProgramNonUniformWorkGroupTests, givenIlProgramWhenCompileProgramThenAllowNonUniformFollowsClStdAndUniformWorkGroupSizeOptions) {
    for (const auto &optionsCase : optionsCases) {
        auto program = createIlProgram();
        ASSERT_NE(nullptr, program);

        EXPECT_EQ(CL_SUCCESS, clCompileProgram(program, 1, &clDeviceId, optionsCase.options, 0, nullptr, nullptr, nullptr, nullptr));
        EXPECT_EQ(optionsCase.expectedAllowNonUniform, allowsNonUniform(program)) << (optionsCase.options ? optionsCase.options : "nullptr");

        clReleaseProgram(program);
    }
}

TEST_F(ClProgramNonUniformWorkGroupTests, givenProgramBuiltWithClStd20WhenRebuiltWithUniformWorkGroupSizeThenNonUniformIsNoLongerAllowed) {
    auto program = createIlProgram();
    ASSERT_NE(nullptr, program);

    EXPECT_EQ(CL_SUCCESS, clBuildProgram(program, 1, &clDeviceId, "-cl-std=CL2.0", nullptr, nullptr));
    EXPECT_TRUE(allowsNonUniform(program));

    EXPECT_EQ(CL_SUCCESS, clBuildProgram(program, 1, &clDeviceId, "-cl-std=CL2.0 -cl-uniform-work-group-size", nullptr, nullptr));
    EXPECT_FALSE(allowsNonUniform(program));

    clReleaseProgram(program);
}

TEST_F(ClProgramNonUniformWorkGroupTests, givenInputProgramsWhenLinkProgramThenNonUniformIsAllowedOnlyWhenEveryInputAllowsIt) {
    auto nonUniformProgram1 = createIlProgram();
    auto nonUniformProgram2 = createIlProgram();
    auto uniformProgram = createIlProgram();
    ASSERT_NE(nullptr, nonUniformProgram1);
    ASSERT_NE(nullptr, nonUniformProgram2);
    ASSERT_NE(nullptr, uniformProgram);
    EXPECT_EQ(CL_SUCCESS, clCompileProgram(nonUniformProgram1, 1, &clDeviceId, "-cl-std=CL2.0", 0, nullptr, nullptr, nullptr, nullptr));
    EXPECT_EQ(CL_SUCCESS, clCompileProgram(nonUniformProgram2, 1, &clDeviceId, "-cl-std=CL3.0", 0, nullptr, nullptr, nullptr, nullptr));
    EXPECT_EQ(CL_SUCCESS, clCompileProgram(uniformProgram, 1, &clDeviceId, "-cl-std=CL1.2", 0, nullptr, nullptr, nullptr, nullptr));

    cl_int errcode = CL_INVALID_VALUE;
    cl_program allNonUniformInputs[] = {nonUniformProgram1, nonUniformProgram2};
    auto linkedNonUniform = clLinkProgram(clContext, 1, &clDeviceId, nullptr, 2, allNonUniformInputs, nullptr, nullptr, &errcode);
    EXPECT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, linkedNonUniform);
    EXPECT_TRUE(allowsNonUniform(linkedNonUniform));

    cl_program mixedInputs[] = {nonUniformProgram1, uniformProgram};
    auto linkedMixed = clLinkProgram(clContext, 1, &clDeviceId, "-cl-std=CL3.0", 2, mixedInputs, nullptr, nullptr, &errcode);
    EXPECT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, linkedMixed);
    EXPECT_FALSE(allowsNonUniform(linkedMixed));

    clReleaseProgram(linkedMixed);
    clReleaseProgram(linkedNonUniform);
    clReleaseProgram(uniformProgram);
    clReleaseProgram(nonUniformProgram2);
    clReleaseProgram(nonUniformProgram1);
}

using ClProgramNativeBinaryLinkTests = ClProgramNonUniformWorkGroupTests;

TEST_F(ClProgramNativeBinaryLinkTests, givenNativeBinaryWithIrSectionWhenCreateProgramWithBinaryThenIrFromBinaryIsCaptured) {
    ZebinTestData::ZebinWithL0TestCommonModule zebin(device->getHwInfo(), {ZebinTestData::AppendElfAdditionalSection::spirv});
    const size_t length = zebin.storage.size();
    const unsigned char *binary = zebin.storage.data();

    cl_int errcode = CL_INVALID_VALUE;
    auto program = clCreateProgramWithBinary(clContext, 1, &clDeviceId, &length, &binary, nullptr, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, program);

    auto leoProgram = castToObject<Program>(program);
    EXPECT_NE(nullptr, leoProgram->getIrBinary());
    EXPECT_NE(0u, leoProgram->getIrBinarySize());

    clReleaseProgram(program);
}

TEST_F(ClProgramNativeBinaryLinkTests, givenNativeBinaryWithoutIrSectionWhenCreateProgramWithBinaryThenProgramIsCreatedWithoutIr) {
    ZebinTestData::ZebinWithL0TestCommonModule zebin(device->getHwInfo());
    const size_t length = zebin.storage.size();
    const unsigned char *binary = zebin.storage.data();

    cl_int errcode = CL_INVALID_VALUE;
    auto program = clCreateProgramWithBinary(clContext, 1, &clDeviceId, &length, &binary, nullptr, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, program);

    auto leoProgram = castToObject<Program>(program);
    EXPECT_EQ(nullptr, leoProgram->getIrBinary());
    EXPECT_EQ(0u, leoProgram->getIrBinarySize());

    clReleaseProgram(program);
}

TEST_F(ClProgramNativeBinaryLinkTests, givenProgramsCreatedFromNativeBinariesWithIrWhenLinkProgramThenLinkedProgramIsExecutable) {
    ZebinTestData::ZebinWithL0TestCommonModule zebin(device->getHwInfo(), {ZebinTestData::AppendElfAdditionalSection::spirv});
    const size_t length = zebin.storage.size();
    const unsigned char *binary = zebin.storage.data();

    cl_int errcode = CL_INVALID_VALUE;
    auto program1 = clCreateProgramWithBinary(clContext, 1, &clDeviceId, &length, &binary, nullptr, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    auto program2 = clCreateProgramWithBinary(clContext, 1, &clDeviceId, &length, &binary, nullptr, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);

    cl_program inputPrograms[] = {program1, program2};
    auto linkedProgram = clLinkProgram(clContext, 1, &clDeviceId, nullptr, 2, inputPrograms, nullptr, nullptr, &errcode);
    EXPECT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, linkedProgram);

    cl_program_binary_type binaryType = CL_PROGRAM_BINARY_TYPE_NONE;
    EXPECT_EQ(CL_SUCCESS, clGetProgramBuildInfo(linkedProgram, clDeviceId, CL_PROGRAM_BINARY_TYPE, sizeof(binaryType), &binaryType, nullptr));
    EXPECT_EQ(static_cast<cl_program_binary_type>(CL_PROGRAM_BINARY_TYPE_EXECUTABLE), binaryType);

    clReleaseProgram(linkedProgram);
    clReleaseProgram(program2);
    clReleaseProgram(program1);
}

TEST_F(ClProgramNonUniformWorkGroupTests, givenNativeBinaryWhenCreateProgramWithBinaryThenAllowNonUniformFollowsBuildOptionsStoredInBinary) {
    struct BinaryCase {
        const char *storedBuildOptions;
        bool expectedAllowNonUniform;
    };
    constexpr BinaryCase binaryCases[] = {
        {nullptr, false},
        {"-cl-std=CL1.2", false},
        {"-cl-std=CL2.0", true},
        {"-cl-std=CL2.0 -cl-uniform-work-group-size", false}};

    for (const auto &binaryCase : binaryCases) {
        ZebinTestData::ValidEmptyProgram<> zebin;
        if (binaryCase.storedBuildOptions) {
            const NEO::ConstStringRef buildOptions = binaryCase.storedBuildOptions;
            zebin.appendSection(NEO::Zebin::Elf::SHT_ZEBIN_MISC, NEO::Zebin::Elf::SectionNames::buildOptions,
                                {reinterpret_cast<const uint8_t *>(buildOptions.data()), buildOptions.size()});
        }
        const size_t length = zebin.storage.size();
        const unsigned char *binary = zebin.storage.data();

        cl_int errcode = CL_INVALID_VALUE;
        auto program = clCreateProgramWithBinary(clContext, 1, &clDeviceId, &length, &binary, nullptr, &errcode);
        ASSERT_EQ(CL_SUCCESS, errcode);
        ASSERT_NE(nullptr, program);
        EXPECT_EQ(binaryCase.expectedAllowNonUniform, allowsNonUniform(program)) << (binaryCase.storedBuildOptions ? binaryCase.storedBuildOptions : "nullptr");

        EXPECT_EQ(CL_SUCCESS, clBuildProgram(program, 1, &clDeviceId, "-cl-std=CL2.0", nullptr, nullptr));
        EXPECT_EQ(binaryCase.expectedAllowNonUniform, allowsNonUniform(program)) << (binaryCase.storedBuildOptions ? binaryCase.storedBuildOptions : "nullptr");

        clReleaseProgram(program);
    }
}

} // namespace ult
} // namespace LEO
} // namespace NEO
