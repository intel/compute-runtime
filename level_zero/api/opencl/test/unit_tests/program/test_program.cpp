/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/platform/platform.h"
#include "level_zero/api/opencl/source/program/program.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/source/module/module.h"

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
} // namespace

// Minimal L0::Module stub that counts destroy() calls and never deletes itself,
// so it is safe to route zeModuleDestroy at it from a stack object.
struct MockTrackedModule : public L0::Module {
    uint32_t destroyCalled = 0u;

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

    std::vector<std::unique_ptr<L0::KernelImmutableData>> emptyKernelImmData;
};

struct MockEmptyContext : public Context {
    MockEmptyContext() : Context(nullptr, nullptr, 0, nullptr, false) {}
};

struct WhiteBoxProgram : public Program {
    using Program::buildModulesForContextDevices;
    using Program::mapModuleBuildResult;
    using Program::moduleHandles;
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

} // namespace ult
} // namespace LEO
} // namespace NEO
